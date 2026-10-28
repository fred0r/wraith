import os
import pty
import pwd
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import warnings
from pathlib import Path

from fake_irc import FakeIRCServer


ROOT = Path(__file__).resolve().parents[2]


class PtyProcess:
    def __init__(self, argv, cwd, env):
        self._cond = threading.Condition()
        self._output = bytearray()
        self._closed = False
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", DeprecationWarning)
            self.pid, self.fd = pty.fork()
        if self.pid == 0:
            try:
                os.chdir(cwd)
                os.execve(argv[0], argv, env)
            except BaseException:
                os._exit(127)
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def _read_loop(self):
        while True:
            try:
                data = os.read(self.fd, 4096)
            except OSError:
                break
            if not data:
                break
            with self._cond:
                self._output.extend(data)
                self._cond.notify_all()

    def output(self):
        with self._cond:
            return bytes(self._output).decode("utf-8", "replace")

    def wait_for_output(self, needle, timeout=10.0):
        deadline = time.monotonic() + timeout
        with self._cond:
            while needle not in bytes(self._output).decode("utf-8", "replace"):
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError(
                        "timed out waiting for %r in process output:\n%s"
                        % (needle, bytes(self._output).decode("utf-8", "replace")[-4000:])
                    )
                self._cond.wait(remaining)

    def write(self, data):
        os.write(self.fd, data.encode())

    def wait(self, timeout=15.0):
        deadline = time.monotonic() + timeout
        while True:
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid == self.pid:
                self._close_fd()
                return os.waitstatus_to_exitcode(status)
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("process %d did not exit" % self.pid)
            time.sleep(min(0.05, remaining))

    def terminate(self):
        if self.pid <= 0:
            return
        try:
            os.kill(self.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            self.wait(3.0)
        except TimeoutError:
            try:
                os.kill(self.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            try:
                os.waitpid(self.pid, 0)
            except ChildProcessError:
                pass
        self._close_fd()

    def _close_fd(self):
        if self._closed:
            return
        self._closed = True
        try:
            os.close(self.fd)
        except OSError:
            pass


class WraithHarness:
    PASSWORD = "WraithTestPass123"
    OWNER_PASSWORD = "OwnerTestPass123"
    PACKNAME = "itest"
    SALT1 = "0123456789abcdef0123456789abcdef"
    SALT2 = "0123456789abcdef"
    BOT = "wraithbot"
    CHANNEL = "#wraith-test"

    def __init__(self, extra_bots=(), groups=None, msgrate="10"):
        self.extra_bots = tuple(extra_bots)
        self.groups = groups
        self.msgrate = msgrate
        self.tempdir = None
        self.binary = None
        self.config_file = None
        self.fake_irc = None
        self.bot = None

    def __enter__(self):
        if os.geteuid() == 0:
            raise RuntimeError("integration tests cannot run as root")
        if not (ROOT / "wraith").is_file():
            raise RuntimeError("wraith is not built; run make dynamic")
        self.tempdir = tempfile.TemporaryDirectory(prefix="wraith-it-")
        try:
            self.binary = Path(self.tempdir.name) / "wraith"
            shutil.copy2(ROOT / "wraith", self.binary)
            self.binary.chmod(0o700)
            self._initialize_binary()
            self._initialize_config()
        except BaseException:
            self.__exit__(None, None, None)
            raise
        return self

    def __exit__(self, exc_type, exc, tb):
        if self.bot:
            self.bot.terminate()
            self.bot = None
        if self.fake_irc:
            self.fake_irc.close()
            self.fake_irc = None
        if self.tempdir:
            self.tempdir.cleanup()
            self.tempdir = None

    def _initialize_binary(self):
        packfile = Path(self.tempdir.name) / "pack.cfg"
        subprocess.run(
            [
                sys.executable,
                str(ROOT / "scripts/mkpackcfg/mkpackcfg.py"),
                "-n",
                self.PACKNAME,
                "-p",
                self.PASSWORD,
                "-o",
                "tester",
                self.OWNER_PASSWORD,
                "-u",
                "hub",
                "127.0.0.1",
                "9",
                "--salt1", self.SALT1,
                "--salt2", self.SALT2,
                "-f",
                str(packfile),
                "--color",
                "never",
            ],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
        result = subprocess.run(
            [str(self.binary), "-q", str(packfile)],
            cwd=self.tempdir.name,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=15,
        )
        if result.returncode != 0:
            raise RuntimeError(
                "pack initialization failed (%d):\n%s"
                % (result.returncode, result.stdout.decode("utf-8", "replace"))
            )

    def _initialize_config(self):
        data_dir = Path(self.tempdir.name) / "data"
        data_dir.mkdir()
        username = pwd.getpwuid(os.geteuid()).pw_name
        config = [
            "! uid %d" % os.geteuid(),
            "! username %s" % username,
            "! homedir %s" % self.tempdir.name,
            "! datadir %s" % data_dir,
            "! autocron 0",
        ]
        config.extend("%s * *" % nick for nick in (self.BOT,) + self.extra_bots)
        self.config_file = Path(self.tempdir.name) / "bot.conf"
        self.config_file.write_text("\n".join(config) + "\n")

        editor = Path(self.tempdir.name) / "editor.sh"
        editor.write_text(
            "#!/bin/sh\nexec cp \"$WRAITH_TEST_CONF\" \"$1\"\n"
        )
        editor.chmod(0o700)

        env = os.environ.copy()
        env.update(
            {
                "EDITOR": str(editor),
                "WRAITH_TEST_CONF": str(self.config_file),
                "TERM": "dumb",
            }
        )
        process = PtyProcess([str(self.binary), "-C"], self.tempdir.name, env)
        process.wait_for_output("Enter your binary password:")
        process.write(self.PASSWORD + "\n")
        result = process.wait()
        if result != 0:
            raise RuntimeError(
                "config initialization failed (%d):\n%s"
                % (result, process.output())
            )

    def start(self):
        self.fake_irc = FakeIRCServer(port=0)
        env = os.environ.copy()
        env["TERM"] = "dumb"
        env["WRAITH_TEST_SERVERS"] = "127.0.0.1:%d" % self.fake_irc.port
        if self.msgrate is not None:
            env["WRAITH_TEST_MSGRATE"] = str(self.msgrate)
        if self.groups is not None:
            env["WRAITH_TEST_GROUPS"] = self.groups
        self.bot = PtyProcess(
            [str(self.binary), "-n", "-t", "-B", self.BOT],
            self.tempdir.name,
            env,
        )
        self.fake_irc.start()
        self.bot.wait_for_output("Enter your binary password:")
        self.bot.write(self.PASSWORD + "\n")
        self.bot.wait_for_output("HQ joined the party line.")
        self.fake_irc.wait_connected()
        self.fake_irc.wait_for_line("MODE %s +" % self.BOT)
        self.console(".console +u")
        return self

    def console(self, command):
        if self.bot is None:
            raise RuntimeError("bot is not running")
        self.bot.write(command + "\n")

    def add_bot_hostmask(self, nick, user, host):
        self.console(".+host %s *!%s@%s" % (nick, user, host))

    def add_channel(self, options="+fastop"):
        start = self.fake_irc.line_count()
        self.console(".+chan %s %s" % (self.CHANNEL, options))
        self.fake_irc.wait_for_new_line("JOIN %s" % self.CHANNEL, start, timeout=15)
        return start

    def wait_ready(self):
        mark = self.fake_irc.line_count()
        self.fake_irc.send_ping()
        self.fake_irc.wait_for_new(
            lambda line: line.startswith("PONG "), mark, timeout=15,
            description="PONG readiness",
        )

    def wait_channel_active(self, start=0):
        sent_start = len(self.fake_irc.sent())
        self.fake_irc.wait_for_new_line("WHO %s" % self.CHANNEL, start, timeout=15)
        self.fake_irc.wait_for_sent(
            lambda line: line.startswith(":fake.irc 315 "), sent_start,
            timeout=15, description="end of WHO",
        )
        self.wait_ready()

    def output(self):
        return self.bot.output() if self.bot else ""

    def wait_output(self, needle, timeout=10):
        if not self.bot:
            raise RuntimeError("bot is not running")
        self.bot.wait_for_output(needle, timeout)


def free_port():
    import socket

    sock = socket.socket()
    try:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]
    finally:
        sock.close()


class LinkedWraithHarness:
    PASSWORD = WraithHarness.PASSWORD
    OWNER_PASSWORD = WraithHarness.OWNER_PASSWORD
    PACKNAME = "itest-link"
    HUB = "hub"
    LEAF = "leaf"
    LEAF2 = "leaf2"
    PEER = "peerbot"
    CHANNEL = "#wraith-test"

    def __init__(self, leaves=None, binaries=None, channel_options=None,
                 bot_groups=None, initially_joined=None, add_hostmasks=False):
        self.leaves = tuple(leaves or (self.LEAF,))
        self.binaries = dict(binaries or {})
        self.channel_options = channel_options or "+fastop groups oldgroup"
        self.bot_groups = dict(bot_groups or {})
        self.initially_joined = tuple(
            self.leaves if initially_joined is None else initially_joined)
        self.add_hostmasks = add_hostmasks
        if len(set(self.leaves)) != len(self.leaves):
            raise ValueError("leaf names must be unique")
        if any(leaf not in self.leaves for leaf in self.initially_joined):
            raise ValueError("initially_joined must name configured leaves")
        self.tempdir = None
        self.root = None
        self.hub_dir = None
        self.leaf_dirs = {}
        self.hub = None
        self.processes = {}
        self.leaf = None
        self.fake_irc = None
        self.botnet_port = None

    def __enter__(self):
        if os.geteuid() == 0:
            raise RuntimeError("integration tests cannot run as root")
        for leaf in self.leaves:
            binary = Path(self.binaries.get(leaf, ROOT / "wraith"))
            if not binary.is_file():
                raise RuntimeError("missing wraith binary for %s: %s" % (leaf, binary))
        self.tempdir = tempfile.TemporaryDirectory(prefix="wraith-link-")
        self.root = Path(self.tempdir.name)
        self.hub_dir = self.root / "hub"
        self.hub_dir.mkdir()
        for leaf in self.leaves:
            directory = self.root / leaf
            directory.mkdir()
            self.leaf_dirs[leaf] = directory
        try:
            self._initialize_instances()
        except BaseException:
            self.__exit__(None, None, None)
            raise
        return self

    def __exit__(self, exc_type, exc, tb):
        for process in list(self.processes.values()) + [self.hub]:
            if process:
                process.terminate()
        self.processes = {}
        self.leaf = self.hub = None
        if self.fake_irc:
            self.fake_irc.close()
            self.fake_irc = None
        if self.tempdir:
            self.tempdir.cleanup()
            self.tempdir = None

    def _initialize_instances(self):
        self.botnet_port = free_port()
        pack = self.root / "pack.cfg"
        subprocess.run(
            [
                sys.executable,
                str(ROOT / "scripts/mkpackcfg/mkpackcfg.py"),
                "-n", self.PACKNAME,
                "-p", self.PASSWORD,
                "-o", "tester", self.OWNER_PASSWORD,
                "-u", self.HUB, "127.0.0.1", str(self.botnet_port),
                "-f", str(pack), "--color", "never",
            ],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
        )
        self._init_binary(self.hub_dir, pack, Path(self.binaries.get(self.HUB, ROOT / "wraith")))
        self._init_config(self.hub_dir, self.HUB)
        for leaf in self.leaves:
            self._init_binary(self.leaf_dirs[leaf], pack,
                Path(self.binaries.get(leaf, ROOT / "wraith")))
            self._init_config(self.leaf_dirs[leaf], leaf)

    def _init_binary(self, directory, pack, source):
        binary = directory / "wraith"
        shutil.copy2(source, binary)
        binary.chmod(0o700)
        result = subprocess.run(
            [str(binary), "-q", str(pack)], cwd=directory,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=15,
        )
        if result.returncode != 0:
            raise RuntimeError("pack initialization failed: %s" %
                               result.stdout.decode("utf-8", "replace"))

    def _init_config(self, directory, bot):
        data_dir = directory / "data"
        data_dir.mkdir()
        username = pwd.getpwuid(os.geteuid()).pw_name
        config = directory / "bot.conf"
        config.write_text("\n".join([
            "! uid %d" % os.geteuid(),
            "! username %s" % username,
            "! homedir %s" % directory,
            "! datadir %s" % data_dir,
            "! autocron 0",
            "%s * *" % bot,
        ]) + "\n")
        editor = directory / "editor.sh"
        editor.write_text('#!/bin/sh\nexec cp "$WRAITH_TEST_CONF" "$1"\n')
        editor.chmod(0o700)
        env = os.environ.copy()
        env.update({"EDITOR": str(editor), "WRAITH_TEST_CONF": str(config),
                    "TERM": "dumb"})
        process = PtyProcess([str(directory / "wraith"), "-C"], str(directory), env)
        process.wait_for_output("Enter your binary password:")
        process.write(self.PASSWORD + "\n")
        if process.wait() != 0:
            raise RuntimeError("config initialization failed: %s" % process.output())

    def _start_process(self, directory, bot, extra_env=None):
        env = os.environ.copy()
        env["TERM"] = "dumb"
        if extra_env:
            env.update(extra_env)
        process = PtyProcess(
            [str(directory / "wraith"), "-n", "-t", "-B", bot],
            str(directory), env,
        )
        process.wait_for_output("Enter your binary password:")
        process.write(self.PASSWORD + "\n")
        process.wait_for_output("HQ joined the party line.")
        return process

    def _leaf_link_masks(self, username):
        return (
            "-telnet!telnet@127.0.0.1 -telnet!~%s@127.0.0.1 "
            "-telnet!%s@127.0.0.1 -telnet!*@127.0.0.1"
            % (username, username)
        )

    def _configure_hub(self):
        username = pwd.getpwuid(os.geteuid()).pw_name
        commands = []
        for leaf in self.leaves:
            commands.append(
                ".newleaf %s %s"
                % (leaf, self._leaf_link_masks(username))
            )
            if self.add_hostmasks:
                commands.append(
                    ".+host %s *!%s@%s.host" % (leaf, leaf, leaf)
                )
        commands.extend([
            ".newleaf %s -telnet!peer@peerhost" % self.PEER,
            ".+host %s *!peer@peerhost" % self.PEER,
            ".+chan %s %s" % (self.CHANNEL, self.channel_options),
            ".set -YES servers 127.0.0.1:%d" % self.fake_irc.port,
            ".set server-cycle-wait 1",
        ])
        for leaf in self.leaves:
            commands.append(".botset %s -YES groups %s"
                            % (leaf, self.bot_groups.get(leaf, "oldgroup")))
        for command in commands:
            self.hub.write(command + "\n")
            time.sleep(0.35)
        self.hub.write(".save\n")
        self.hub.wait_for_output("Saving user file...success.", timeout=15)

    def start(self):
        self.fake_irc = FakeIRCServer(port=0)
        self.fake_irc.start()
        self.hub = self._start_process(self.hub_dir, self.HUB)
        self._configure_hub()
        for leaf in self.leaves:
            self.processes[leaf] = self._start_process(
                self.leaf_dirs[leaf], leaf, {"WRAITH_TEST_MSGRATE": "10"}
            )
        self.leaf = self.processes[self.leaves[0]]
        for leaf in self.leaves:
            process = self.processes[leaf]
            process.write(".console +u\n")
            try:
                process.wait_for_output("Userfile loaded", timeout=45)
            except TimeoutError as exc:
                raise RuntimeError(
                    "%s did not load the userfile\n"
                    "--- %s output ---\n%s\n"
                    "--- hub output ---\n%s"
                    % (leaf, leaf, process.output(), self.hub.output())
                ) from exc
            self.fake_irc.wait_connected(timeout=45, nick=leaf)
            self.fake_irc.wait_for_line("MODE %s +" % leaf, timeout=20, nick=leaf)
            if leaf in self.initially_joined:
                self.fake_irc.wait_for_line(
                    "JOIN %s" % self.CHANNEL, timeout=45, nick=leaf)
                self.fake_irc.wait_for_line(
                    "WHO %s" % self.CHANNEL, timeout=45, nick=leaf)
                self.fake_irc.wait_for_sent(
                    lambda line: line.startswith(":fake.irc 315 "), 0,
                    timeout=20, description="end of linked WHO", nick=leaf,
                )
            self._wait_ready(leaf)
        for leaf in self.initially_joined:
            self.fake_irc.send_mode(self.CHANNEL, "+o", leaf)
        for leaf in self.leaves:
            self._wait_ready(leaf)
        return self

    def _wait_ready(self, nick=None):
        nick = nick or self.leaves[0]
        mark = self.fake_irc.line_count(nick)
        self.fake_irc.send_ping(nick=nick)
        self.fake_irc.wait_for_new(
            lambda line: line.startswith("PONG "), mark, timeout=15,
            description="PONG readiness", nick=nick,
        )

    def leaf_process(self, nick=None):
        return self.processes[nick or self.leaves[0]]

    def console(self, nick, command):
        self.leaf_process(nick).write(command + "\n")

    def leaf_console(self, command, nick=None):
        self.console(nick or self.leaves[0], command)

    def wait_ready(self, nick=None):
        self._wait_ready(nick)

    def wait_output(self, needle, timeout=15, nick=None):
        self.leaf_process(nick).wait_for_output(needle, timeout)

    def roles(self, chan, timeout=30, nick=None):
        nick = nick or self.leaves[0]
        process = self.leaf_process(nick)
        mark = len(process.output())
        process.write(".roles %s\n" % chan)
        deadline = time.monotonic() + timeout
        while True:
            tail = process.output()[mark:]
            if "Roles for %s" % chan in tail:
                return tail
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(
                    "roles were not calculated for %s; output:\n%s"
                    % (chan, process.output()[-3000:])
                )
            time.sleep(min(0.25, remaining))

    def wait_roles(self, chan, timeout=30, nick=None):
        deadline = time.monotonic() + timeout
        while True:
            tail = self.roles(chan, timeout=max(1.0, deadline - time.monotonic()), nick=nick)
            if "not yet calculated" not in tail:
                return tail
            if time.monotonic() >= deadline:
                raise TimeoutError("roles were not calculated for %s" % chan)
            time.sleep(0.5)
