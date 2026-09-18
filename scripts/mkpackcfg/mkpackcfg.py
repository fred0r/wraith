#!/usr/bin/env python3
"""Generate a wraith pack.cfg file.

Usage:
  ./mkpackcfg.py -n mypack -p mybinarypass -o alice mypass -u hub1 hub.example.com 1234
  ./mkpackcfg.py -i -f mypack.cfg
  ./mkpackcfg.py -n mypack -p mypass -o alice mypass -u hub1 h.example.com 1234 --dry-run

See --help for full options.
"""

import argparse
import hashlib
import os
import secrets
import string
import sys


VERSION = "1.0"
ALPHANUM = string.ascii_letters + string.digits
SALT1_LEN = 32
SALT2_LEN = 16
SHA1_SALT_LEN = 5

BANNER = r"""                        __  __   __
__  _  ______________  |__|/  |_|  |__
\ \/ \/ /\_  __ \__  \ |  \    _\  |  \
 \     /  |  | \// __ \|  ||  | |   \  \
  \/\_/   |__|  (____  /__||__| |___|  /
                     \/              \/
       http://wraith.botpack.net
            @wraithbotpack"""


# -- terminal color -----------------------------------------------------------

def can_color():
    return hasattr(sys.stdout, "isatty") and sys.stdout.isatty()


class Dye:
    def __init__(self, enable):
        self.enable = enable

    def _c(self, code, s):
        if not self.enable:
            return s
        return f"\033[{code}m{s}\033[m"

    def green(self, s):    return self._c("32", s)
    def yellow(self, s):   return self._c("33", s)
    def red(self, s):      return self._c("31", s)
    def cyan(self, s):     return self._c("36", s)
    def bold(self, s):     return self._c("1", s)
    def dim(self, s):      return self._c("2", s)
    def header(self, s):   return self._c("1;34", s)


# -- core helpers -------------------------------------------------------------

def salted_sha1(password: str) -> str:
    salt = ''.join(secrets.choice(ALPHANUM) for _ in range(SHA1_SALT_LEN))
    digest = hashlib.sha1((salt + password).encode()).hexdigest()
    return f"+{salt}${digest}"


def rand_str(length: int) -> str:
    return ''.join(secrets.choice(ALPHANUM) for _ in range(length))


def is_valid_host(s):
    parts = s.split('.')
    if len(parts) == 4:
        try:
            return all(0 <= int(p) <= 255 for p in parts)
        except ValueError:
            pass
    return 1 <= len(s) <= 255 and ' ' not in s


# -- validation ---------------------------------------------------------------

def validate(args, dye):
    errors = []

    if not args.packname:
        errors.append("PACKNAME is required")
    elif len(args.packname) > 32:
        errors.append("PACKNAME must be 32 characters or less")
    elif ' ' in args.packname:
        errors.append("PACKNAME must not contain spaces")

    if not args.binarypass:
        errors.append("BINARYPASS is required")
    elif len(args.binarypass) < 8:
        errors.append("BINARYPASS must be at least 8 characters")
    elif len(args.binarypass) > 64:
        errors.append("BINARYPASS must be at most 64 characters")

    if len(args.dccprefix) != 1:
        errors.append("DCCPREFIX must be exactly one character")

    if not args.owners:
        errors.append("At least one OWNER is required")
    else:
        for entry in args.owners:
            if len(entry) < 2:
                errors.append("OWNER requires at least nick and password")
            elif len(entry[1]) < 8:
                errors.append(f"Password for owner '{entry[0]}' must be at least 8 characters")
            elif len(entry[1]) > 64:
                errors.append(f"Password for owner '{entry[0]}' must be at most 64 characters")

    if not args.hubs:
        errors.append("At least one HUB is required")
    else:
        for entry in args.hubs:
            if len(entry) < 3:
                errors.append("HUB requires nick, host, and port")
            else:
                if not is_valid_host(entry[1]):
                    errors.append(f"Invalid host '{entry[1]}' for hub {entry[0]}")
                try:
                    port = int(entry[2])
                    if port < 1 or port > 65535:
                        errors.append(f"Invalid port {port} for hub {entry[0]}")
                except ValueError:
                    errors.append(f"Port '{entry[2]}' for hub {entry[0]}' is not a number")

    if args.salt1 and len(args.salt1) != SALT1_LEN:
        errors.append(f"SALT1 must be exactly {SALT1_LEN} characters (got {len(args.salt1)})")
    if args.salt2 and len(args.salt2) != SALT2_LEN:
        errors.append(f"SALT2 must be exactly {SALT2_LEN} characters (got {len(args.salt2)})")

    return errors


# -- config generation --------------------------------------------------------

def build_config(args):
    return {
        "packname": args.packname,
        "binarypass": salted_sha1(args.binarypass),
        "dccprefix": args.dccprefix,
        "owners": [
            {
                "nick": o[0],
                "hash": salted_sha1(o[1]),
                "hosts": o[2:] if len(o) > 2 else [],
            }
            for o in args.owners
        ],
        "hubs": [
            {"nick": u[0], "host": u[1], "port": u[2]}
            for u in args.hubs
        ],
        "salt1": args.salt1 or rand_str(SALT1_LEN),
        "salt2": args.salt2 or rand_str(SALT2_LEN),
    }


def write_config(cfg, outfile):
    print("PACKNAME", cfg["packname"], file=outfile)
    print("BINARYPASS", cfg["binarypass"], file=outfile)
    print("DCCPREFIX", cfg["dccprefix"], file=outfile)
    for o in cfg["owners"]:
        rest = " " + " ".join(o["hosts"]) if o["hosts"] else ""
        print(f"OWNER {o['nick']} {o['hash']}{rest}", file=outfile)
    for u in cfg["hubs"]:
        print(f"HUB {u['nick']} {u['host']} {u['port']}", file=outfile)
    print("SALT1", cfg["salt1"], file=outfile)
    print("SALT2", cfg["salt2"], file=outfile)


# -- display helpers ----------------------------------------------------------

def show_summary(cfg, output_path, dry_run, dye):
    print()
    print(dye.header("== Configuration Summary =="))
    print(f"  {dye.bold('PACKNAME:')}    {cfg['packname']}")
    print(f"  {dye.bold('BINARYPASS:')}  {dye.dim(cfg['binarypass'])}")
    print(f"  {dye.bold('DCCPREFIX:')}   {cfg['dccprefix']}")
    print(f"  {dye.bold('OWNERS:')}      {', '.join(o['nick'] for o in cfg['owners'])}")
    for o in cfg['owners']:
        h = " ".join(o['hosts']) if o['hosts'] else "(no hostmask)"
        print(f"                  {dye.dim(o['nick'] + ' -> ' + o['hash'] + ' ' + h)}")
    print(f"  {dye.bold('HUBS:')}        {', '.join(u['nick'] for u in cfg['hubs'])}")
    for u in cfg['hubs']:
        print(f"                  {dye.dim(u['nick'] + ' ' + u['host'] + ':' + u['port'])}")
    s1label = dye.bold("SALT1:")
    s2label = dye.bold("SALT2:")
    s1dim = dye.dim(f"({len(cfg['salt1'])} chars)")
    s2dim = dye.dim(f"({len(cfg['salt2'])} chars)")
    print(f"  {s1label}      {cfg['salt1']}  {s1dim}")
    print(f"  {s2label}      {cfg['salt2']}  {s2dim}")
    if output_path:
        print(f"  {dye.bold('Output:')}     {output_path}")
    print()


def show_status(msg, dye):
    print(f"  {dye.green('+')} {msg}")


# -- interactive mode ---------------------------------------------------------



def prompt_str(prompt, default=None, dye=None):
    if default is not None:
        full = f"{prompt} [{default}]: "
    else:
        full = f"{prompt}: "
    val = input(full).strip()
    if not val and default is not None:
        return default
    return val


def interactive(args, dye):
    print(dye.dim(BANNER))
    print(f"  {dye.dim('mkpackcfg.py v' + VERSION)}")
    print()
    print(dye.dim("  - Passwords: 8-64 characters"))
    print(dye.dim("  - Hub host: IP or FQDN (FQDN recommended - hub can be moved easier)"))
    print()
    print(dye.header("== Required Settings =="))

    while not args.packname:
        val = prompt_str("PACKNAME (no spaces, max 32 chars)")
        if val and len(val) <= 32 and ' ' not in val:
            args.packname = val
        elif not val:
            pass
        elif ' ' in val:
            print(f"  {dye.red('!')} PACKNAME must not contain spaces" if dye else "  ! PACKNAME must not contain spaces")
        else:
            print(f"  {dye.red('!')} PACKNAME too long (max 32 chars)" if dye else "  ! PACKNAME too long (max 32 chars)")

    import getpass
    while not args.binarypass:
        val = getpass.getpass("  BINARYPASS (cleartext): ").strip()
        if not val:
            continue
        if len(val) < 8:
            print(f"  {dye.red('!')} BINARYPASS must be at least 8 characters")
            continue
        if len(val) > 64:
            print(f"  {dye.red('!')} BINARYPASS must be at most 64 characters")
            continue
        confirm = getpass.getpass("  Confirm BINARYPASS: ").strip()
        if val != confirm:
            print(f"  {dye.red('!')} Passwords do not match")
            continue
        args.binarypass = val

    while True:
        val = prompt_str("  DCCPREFIX (single character)", default=args.dccprefix)
        if len(val) != 1:
            print(f"  {dye.red('!')} DCCPREFIX must be exactly one character")
            continue
        args.dccprefix = val
        break

    print()
    print(dye.header("== Owners =="))
    if not args.owners:
        n = 1
        while True:
            print(f"  {dye.bold(f'--- Owner #{n} ---')}")
            while True:
                nick = prompt_str("  Nick")
                if not nick:
                    if n == 1:
                        print(f"  {dye.red('!')} At least one owner is required")
                        continue
                break
            if not nick:
                break
            while True:
                passwd = getpass.getpass("  Password: ").strip()
                if not passwd:
                    print(f"  {dye.red('!')} Password is required")
                    continue
                if len(passwd) < 8:
                    print(f"  {dye.red('!')} Password must be at least 8 characters")
                    continue
                if len(passwd) > 64:
                    print(f"  {dye.red('!')} Password must be at most 64 characters")
                    continue
                confirm = getpass.getpass("  Confirm Password: ").strip()
                if passwd != confirm:
                    print(f"  {dye.red('!')} Passwords do not match")
                    continue
                break
            host = prompt_str("  Hostmask (optional)")
            entry = [nick, passwd]
            if host:
                entry.append(host)
            args.owners.append(entry)
            n += 1
            ok = prompt_str("  Add another owner?", default="n").lower()
            if ok != "y":
                break

    print()
    print(dye.header("== Hubs =="))
    if not args.hubs:
        n = 1
        while True:
            print(f"  {dye.bold(f'--- Hub #{n} ---')}")
            while True:
                nick = prompt_str("  Nick")
                if not nick:
                    if n == 1:
                        print(f"  {dye.red('!')} At least one hub is required")
                        continue
                break
            if not nick:
                break
            while True:
                host = prompt_str("  Host")
                if not host:
                    print(f"  {dye.red('!')} Host is required")
                    continue
                if not is_valid_host(host):
                    print(f"  {dye.red('!')} Invalid IP or hostname")
                    continue
                break
            while True:
                port = prompt_str("  Port")
                if not port:
                    print(f"  {dye.red('!')} Port is required")
                    continue
                try:
                    p = int(port)
                    if p < 1 or p > 65535:
                        print(f"  {dye.red('!')} Port must be 1-65535")
                        continue
                except ValueError:
                    print(f"  {dye.red('!')} Port must be a number")
                    continue
                break
            args.hubs.append([nick, host, port])
            n += 1
            ok = prompt_str("  Add another hub?", default="n").lower()
            if ok != "y":
                break

    print()
    print(dye.header("== Optional Settings =="))

    if not args.salt1:
        val = prompt_str(f"SALT1 ({SALT1_LEN} chars)", default="<random>")
        args.salt1 = None if val == "<random>" or not val else val

    if not args.salt2:
        val = prompt_str(f"SALT2 ({SALT2_LEN} chars)", default="<random>")
        args.salt2 = None if val == "<random>" or not val else val


# -- entry point --------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Generate a wraith pack.cfg file",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Examples:\n"
            "  %(prog)s -n mypack -p binarypass -o alice secret -u hub1 h.example.com 1234\n"
            "  %(prog)s -i -f mypack.cfg\n"
            "  %(prog)s -n mypack -p pass -o alice pass -u hub1 h.com 1234 --dry-run\n"
        ),
    )
    parser.add_argument("-n", "--packname", help="pack name (no spaces, max 32 chars)")
    parser.add_argument("-p", "--binarypass", help="binary password (cleartext)")
    parser.add_argument("-d", "--dccprefix", default=".",
                        help="DCC command prefix (default: .)")
    parser.add_argument("-o", "--owner", dest="owners",
                        action="append", nargs="+", default=[],
                        help="owner (nick pass [host...])")
    parser.add_argument("-u", "--hub", dest="hubs",
                        action="append", nargs="+", default=[],
                        help="hub (nick host port)")
    parser.add_argument("--salt1", help=f"salt1 ({SALT1_LEN} chars)")
    parser.add_argument("--salt2", help=f"salt2 ({SALT2_LEN} chars)")
    parser.add_argument("-f", "--output",
                        help="write to FILE instead of stdout")
    parser.add_argument("-i", "--interactive", action="store_true",
                        help="prompt for missing required fields")
    parser.add_argument("--dry-run", action="store_true",
                        help="show the config without writing")
    parser.add_argument("--color", choices=["auto", "always", "never"],
                        default="auto", help="colorized output (default: auto)")
    parser.add_argument("--version", action="store_true",
                        help="show version and exit")

    args = parser.parse_args()
    dye = Dye(
        enable=(args.color == "always") or (args.color == "auto" and can_color())
    )

    if args.version:
        print(f"mkpackcfg.py v{VERSION}")
        return

    if args.interactive:
        interactive(args, dye)

    errors = validate(args, dye)
    if errors:
        parser.print_help()
        print()
        print(f"\n{dye.red('Validation errors:') if dye.enable else 'Validation errors:'}")
        for e in errors:
            print(f"  {dye.red('!') if dye.enable else ' !'} {e}")
        sys.exit(1)

    cfg = build_config(args)

    if args.dry_run:
        show_summary(cfg, args.output, dry_run=True, dye=dye)
        print(dye.dim("  (dry run -- nothing written)"))
        return

    if args.output:
        if os.path.exists(args.output):
            print(f"\n{dye.yellow('!')} {args.output} exists, overwrite?" if dye.enable
                  else f"\n! {args.output} exists, overwrite?")
            try:
                ok = input("  Proceed? [y/N]: ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                ok = "n"
            if ok != "y":
                print("  cancelled")
                return

        show_summary(cfg, args.output, dry_run=False, dye=dye)
        with open(args.output, "w") as f:
            write_config(cfg, f)
        show_status(f"wrote {args.output}", dye)
    else:
        show_summary(cfg, None, dry_run=False, dye=dye)
        print(dye.dim("  --- stdout ---"))
        write_config(cfg, sys.stdout)


if __name__ == "__main__":
    main()
