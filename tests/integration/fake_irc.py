import socketserver
import threading
import time


class _ThreadingTCPServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


class _Client:
    def __init__(self, handler):
        self.handler = handler
        self.nick = None
        self.user = None
        self.welcomed = False
        self.received = []
        self.sent = []


class _Handler(socketserver.StreamRequestHandler):
    def handle(self):
        fake = self.server.fake
        client = fake._attach(self)
        try:
            while True:
                raw = self.rfile.readline()
                if not raw:
                    break
                line = raw.decode("utf-8", "replace").rstrip("\r\n")
                if line:
                    fake._on_line(client, line)
        except (ConnectionResetError, BrokenPipeError, RuntimeError, ValueError):
            pass
        finally:
            fake._detach(client)


class FakeIRCServer:
    def __init__(self, host="127.0.0.1", port=6667):
        self._cond = threading.Condition(threading.RLock())
        self._server = _ThreadingTCPServer((host, port), _Handler, bind_and_activate=False)
        self._server.fake = self
        self._server.server_bind()
        self._server.server_activate()
        self.port = self._server.server_address[1]
        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._clients = []
        self._received = []
        self._sent = []
        self.channels = {}
        self.join_errors = {}
        self.defer_who = set()
        self._who_deferred = []
        self._started = False

    def start(self):
        self._started = True
        self._thread.start()

    def close(self):
        with self._cond:
            self._cond.notify_all()
        if self._started:
            self._server.shutdown()
        self._server.server_close()
        with self._cond:
            clients = list(self._clients)
            self._clients = []
        for client in clients:
            try:
                client.handler.finish()
            except OSError:
                pass

    def _attach(self, handler):
        client = _Client(handler)
        with self._cond:
            self._clients.append(client)
            self._cond.notify_all()
        return client

    def _detach(self, client):
        with self._cond:
            if client in self._clients:
                self._clients.remove(client)
            self._cond.notify_all()

    def _client(self, nick=None):
        with self._cond:
            if nick is None:
                return self._clients[0] if self._clients else None
            for client in self._clients:
                if client.nick and client.nick.lower() == nick.lower():
                    return client
        return None

    def _write(self, client, line):
        if client is None:
            raise RuntimeError("no IRC client connected")
        self._sent_append(client, line)
        try:
            client.handler.wfile.write((line + "\r\n").encode("utf-8"))
            client.handler.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, AttributeError, ValueError):
            pass

    def _sent_append(self, client, line):
        with self._cond:
            client.sent.append(line)
            self._sent.append(line)
            self._cond.notify_all()

    def send(self, line, nick=None):
        if nick is not None:
            client = self._client(nick)
            if client is None:
                raise RuntimeError("no IRC client named %s" % nick)
            self._write(client, line)
            return
        with self._cond:
            clients = list(self._clients)
        if not clients:
            raise RuntimeError("no IRC client connected")
        for client in clients:
            self._write(client, line)

    def broadcast(self, line):
        self.send(line)

    def received(self, nick=None):
        if nick is None:
            with self._cond:
                return list(self._received)
        client = self._client(nick)
        if client is None:
            return []
        with self._cond:
            return list(client.received)

    def sent(self, nick=None):
        if nick is None:
            with self._cond:
                return list(self._sent)
        client = self._client(nick)
        if client is None:
            return []
        with self._cond:
            return list(client.sent)

    def _received_lines(self, nick):
        if nick is None:
            return list(self._received)
        client = self._client(nick)
        return list(client.received) if client else []

    def _sent_lines(self, nick):
        if nick is None:
            return list(self._sent)
        client = self._client(nick)
        return list(client.sent) if client else []

    def wait_for(self, predicate, timeout=5.0, description="IRC line", nick=None):
        deadline = time.monotonic() + timeout
        with self._cond:
            while True:
                lines = self._received_lines(nick)
                for line in lines:
                    if predicate(line):
                        return line
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError(
                        "timed out waiting for %s; received:\n%s"
                        % (description, "\n".join(lines[-40:]))
                    )
                self._cond.wait(remaining)

    def wait_for_new(self, predicate, start, timeout=5.0, description="IRC line", nick=None):
        deadline = time.monotonic() + timeout
        with self._cond:
            while True:
                lines = self._received_lines(nick)
                for line in lines[start:]:
                    if predicate(line):
                        return line
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError(
                        "timed out waiting for %s; received:\n%s"
                        % (description, "\n".join(lines[-40:]))
                    )
                self._cond.wait(remaining)

    def wait_for_line(self, prefix, timeout=5.0, nick=None):
        return self.wait_for(
            lambda line: line.startswith(prefix), timeout, prefix, nick)

    def wait_for_new_line(self, prefix, start, timeout=5.0, nick=None):
        return self.wait_for_new(
            lambda line: line.startswith(prefix), start, timeout, prefix, nick)

    def wait_for_sent(self, predicate, start, timeout=5.0, description="sent IRC line", nick=None):
        deadline = time.monotonic() + timeout
        with self._cond:
            while True:
                lines = self._sent_lines(nick)
                for line in lines[start:]:
                    if predicate(line):
                        return line
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError(
                        "timed out waiting for %s; sent:\n%s"
                        % (description, "\n".join(lines[-40:]))
                    )
                self._cond.wait(remaining)

    def send_ping(self, token="ready", nick=None):
        self.send(":fake.irc PING :%s" % token, nick)

    def wait_connected(self, timeout=10.0, nick=None):
        deadline = time.monotonic() + timeout
        with self._cond:
            while True:
                if nick is None:
                    connected = any(c.welcomed for c in self._clients)
                else:
                    client = self._client(nick)
                    connected = bool(client and client.welcomed)
                if connected:
                    return
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError("bot did not complete IRC registration")
                self._cond.wait(remaining)

    def line_count(self, nick=None):
        if nick is None:
            with self._cond:
                return len(self._received)
        client = self._client(nick)
        if client is None:
            return 0
        with self._cond:
            return len(client.received)

    def _on_line(self, client, line):
        with self._cond:
            client.received.append(line)
            self._received.append(line)
            self._cond.notify_all()

        prefix = None
        rest = line
        if rest.startswith(":"):
            fields = rest[1:].split(" ", 1)
            prefix = fields[0]
            rest = fields[1] if len(fields) > 1 else ""
        command, _, params = rest.partition(" ")
        command = command.upper()

        if command == "NICK":
            with self._cond:
                client.nick = params.strip().lstrip(":")
            self._maybe_welcome(client)
        elif command == "USER":
            with self._cond:
                client.user = params
            self._maybe_welcome(client)
        elif command == "PING":
            token = params.strip().lstrip(":")
            self._write(client, ":fake.irc PONG fake.irc :%s" % token)
        elif command == "JOIN":
            self._handle_join(client, prefix, params)
        elif command == "PART":
            self._handle_part(client, prefix, params)
        elif command == "QUIT":
            self._handle_quit(prefix)
        elif command == "WHO":
            self._handle_who(client, params)
        elif command == "WHOIS":
            self._handle_whois(client, params)
        elif command == "USERHOST":
            self._handle_userhost(client, params)
        elif command == "MODE":
            self._handle_mode(prefix, params, client)
        elif command == "KICK":
            self._handle_kick(prefix, params)

    def _maybe_welcome(self, client):
        with self._cond:
            if client.welcomed or not client.nick or client.user is None:
                return
            nick = client.nick
            client.welcomed = True
            self._cond.notify_all()
        self.send(":fake.irc 001 %s :Welcome to the test IRC network" % nick, nick)
        self.send(":fake.irc 004 %s fake.irc test-ircd-1.0 iow biklmnopst" % nick, nick)
        self.send(
            ":fake.irc 005 %s CHANTYPES=# PREFIX=(ov)@+ CHANMODES=b,k,l,imnpst "
            "MODES=4 NICKLEN=32 CASEMAPPING=rfc1459 :are supported by this server"
            % nick,
            nick,
        )
        self.send(":fake.irc 375 %s :- fake.irc Message of the Day -" % nick, nick)
        self.send(":fake.irc 372 %s :- deterministic fake IRC server" % nick, nick)
        self.send(":fake.irc 376 %s :End of /MOTD command." % nick, nick)

    def _handle_join(self, client, prefix, params):
        member = None
        if prefix:
            nick = prefix.split("!", 1)[0]
        else:
            nick = client.nick
            if not nick:
                return
            prefix = "%s!%s@%s.host" % (nick, nick, nick)
            member = {
                "nick": nick, "user": nick, "host": "%s.host" % nick,
                "modes": "",
            }
        if not nick:
            return
        chans = params.strip().lstrip(":").split()[0].split(",")
        if chans[0] in self.join_errors:
            numeric, text = self.join_errors[chans[0]]
            self.send(
                ":fake.irc %s %s %s :%s" % (numeric, nick, chans[0], text),
                nick,
            )
            return
        if member is None:
            member = self._ensure_member(chans[0], nick)
        for chan in chans:
            self.channels.setdefault(chan, {})[nick] = member
            self.send(":%s JOIN %s" % (prefix, chan))

    def _handle_part(self, client, prefix, params):
        if prefix:
            nick = prefix.split("!", 1)[0]
        else:
            nick = client.nick
        if not nick:
            return
        chan = params.split(" ", 1)[0]
        with self._cond:
            self.channels.get(chan, {}).pop(nick, None)
        if prefix:
            self.send(":%s PART %s" % (prefix, chan))
        else:
            self.send(":%s!~user@host PART %s" % (nick, chan))

    def _handle_quit(self, prefix):
        if prefix:
            nick = prefix.split("!", 1)[0]
        else:
            return
        with self._cond:
            for members in self.channels.values():
                members.pop(nick, None)

    def _handle_who(self, client, params):
        chan = params.strip().lstrip(":")
        if chan in self.defer_who:
            with self._cond:
                self._who_deferred.append((chan, client))
            return
        self.send_who(chan, client=client)

    def _handle_whois(self, client, params):
        nick = params.split()[0].lstrip(":")
        target = client.nick
        self.send(":fake.irc 311 %s %s user host * :realname" % (target, nick), client.nick)
        self.send(":fake.irc 312 %s %s fake.irc :fake server" % (target, nick), client.nick)
        self.send(":fake.irc 318 %s %s :End of /WHOIS list." % (target, nick), client.nick)

    def _handle_userhost(self, client, params):
        nick = params.split()[0].lstrip(":")
        target = client.nick
        self.send(":fake.irc 302 %s :%s=+~user@host" % (target, nick), client.nick)

    def _handle_mode(self, prefix, params, client):
        fields = params.split()
        if len(fields) < 2:
            return
        target = fields[0]
        if not target.startswith("#"):
            return
        sign = "+"
        param_index = 2
        ack_modes = ""
        ack_params = []
        for mode in fields[1]:
            if mode in "+-":
                sign = mode
                continue
            if mode in "ovbeIkl":
                if param_index >= len(fields):
                    break
                param = fields[param_index]
                param_index += 1
                if mode == "o":
                    with self._cond:
                        member = self.channels.get(target, {}).get(param)
                    if member:
                        member["modes"] = "+o" if sign == "+" else ""
                    ack_modes += sign + "o"
                    ack_params.append(param)
        if prefix is None and client is not None and client.nick:
            prefix = "%s!%s@%s.host" % (client.nick, client.nick, client.nick)

        if prefix:
            self.send(":%s MODE %s" % (prefix, params))
        elif ack_modes:
            self.send(":fake.irc MODE %s %s %s" % (target, ack_modes, " ".join(ack_params)))

    def _handle_kick(self, prefix, params):
        fields = params.split()
        if len(fields) >= 2:
            with self._cond:
                self.channels.get(fields[0], {}).pop(fields[1], None)

    def _ensure_member(self, chan, nick):
        with self._cond:
            members = self.channels.setdefault(chan, {})
            if nick not in members:
                members[nick] = {"nick": nick, "user": "user", "host": "host", "modes": ""}
            return members[nick]

    def fail_join(self, chan, numeric, text):
        self.join_errors[chan] = (numeric, text)

    def clear_join_error(self, chan):
        self.join_errors.pop(chan, None)

    def add_member(self, chan, nick, user="user", host="host", modes="", send_join=True):
        member = {"nick": nick, "user": user, "host": host, "modes": modes}
        with self._cond:
            self.channels.setdefault(chan, {})[nick] = member
        if send_join:
            prefix = "%s!%s@%s" % (nick, user, host)
            self.send(":%s JOIN %s" % (prefix, chan))
        return member

    def remove_member(self, chan, nick, reason="bye"):
        with self._cond:
            member = self.channels.get(chan, {}).pop(nick, None)
        if member:
            prefix = "%s!%s@%s" % (member["nick"], member["user"], member["host"])
            self.send(":%s QUIT :%s" % (prefix, reason))

    def send_who(self, target_name, end=True, client=None):
        with self._cond:
            target_client = client if client is not None else (self._clients[0] if self._clients else None)
            target = target_client.nick if target_client else None
            if target_name in self.channels:
                channel = target_name
                members = list(self.channels[channel].values())
            else:
                channel = None
                member = None
                for name, members_by_nick in self.channels.items():
                    if target_name in members_by_nick:
                        channel = name
                        member = members_by_nick[target_name]
                        break
                members = [member] if member else []
        if target_client is None:
            return
        for member in members:
            self.send(
                ":fake.irc 352 %s %s %s %s fake.irc %s %s :0 realname"
                % (
                    target,
                    channel,
                    member["user"],
                    member["host"],
                    member["nick"],
                    member["modes"],
                ),
                target_client.nick,
            )
        if end:
            self.send_who_end(target_name, target_client)

    def send_who_end(self, target_name, client=None):
        target_client = client if client is not None else self._client()
        if target_client is None or target_client.nick is None:
            return
        self.send(
            ":fake.irc 315 %s %s :End of /WHO list."
            % (target_client.nick, target_name),
            target_client.nick,
        )

    def wait_for_cookie(self, chan, nick, start=0, timeout=15.0, client_nick=None):
        line = self.wait_for_new(
            lambda value: (
                len(value.split()) >= 4
                and value.startswith("MODE %s " % chan)
                and "+o" in value.split()[2]
                and nick in value.split()
            ),
            start,
            timeout,
            "cookie for %s" % nick,
            client_nick,
        )
        fields = line.split()
        index = fields.index(nick)
        if index + 1 >= len(fields):
            raise AssertionError("cookie missing from %r" % line)
        return fields[index + 1]

    def kick_member(self, chan, nick, by="fake.irc", reason="test"):
        with self._cond:
            self.channels.get(chan, {}).pop(nick, None)
        self.send(":%s KICK %s %s :%s" % (by, chan, nick, reason))

    def part_member(self, chan, nick, reason="test"):
        with self._cond:
            member = self.channels.get(chan, {}).pop(nick, None)
        if not member:
            return
        self.send(":%s!%s@%s PART %s :%s" % (
            member["nick"], member["user"], member["host"], chan, reason))
        with self._cond:
            clients = list(self._clients)
        for client in clients:
            if client.nick:
                self.send(
                    ":fake.irc 315 %s %s :End of /WHO list."
                    % (client.nick, chan), client.nick)

    def nick_member(self, old, new, user="user", host="host"):
        with self._cond:
            found = False
            for members in self.channels.values():
                member = members.pop(old, None)
                if member:
                    member["nick"] = new
                    members[new] = member
                    found = True
        if found:
            self.send(":%s!%s@%s NICK :%s" % (old, user, host, new))

    def drop_client(self, nick=None):
        client = self._client(nick)
        if client is None:
            return
        with self._cond:
            if client in self._clients:
                self._clients.remove(client)
            self._cond.notify_all()
        try:
            client.handler.finish()
        except OSError:
            pass

    def set_member_host(self, chan, nick, host):
        with self._cond:
            member = self.channels.get(chan, {}).get(nick)
            if member:
                member["host"] = host

    def send_mode(self, chan, modes, nick):
        with self._cond:
            member = self.channels.get(chan, {}).get(nick)
        self.send(":fake.irc MODE %s %s %s" % (chan, modes, nick))
        if member:
            if modes == "+o":
                member["modes"] = "+o"
            elif modes == "-o":
                member["modes"] = ""

    def send_opper_mode(self, chan, opper, target, cookie=None):
        with self._cond:
            member = self.channels.get(chan, {}).get(target)
            opper_member = self.channels.get(chan, {}).get(opper)
        user = opper_member["user"] if opper_member else "opp"
        host = opper_member["host"] if opper_member else "opphost"
        if cookie is None:
            line = ":%s!%s@%s MODE %s +o %s" % (opper, user, host, chan, target)
        else:
            line = ":%s!%s@%s MODE %s +o-b %s %s" % (
                opper, user, host, chan, target, cookie,
            )
        self.send(line)
        if member:
            member["modes"] = "+o"

    def finish_deferred_who(self, end=True):
        with self._cond:
            deferred = list(self._who_deferred)
            self._who_deferred.clear()
        for chan, client in deferred:
            self.send_who(chan, end=end, client=client)
