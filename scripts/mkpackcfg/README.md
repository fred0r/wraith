mkpackcfg — Wraith Pack Config Generator
==========================================

Standalone Python 3 CLI tool that generates wraith ``pack.cfg`` files.

Replicates the functionality described at:
  https://wraith.botpack.net/wraith.botpack.net/wiki/PackConfig.html

Usage
-----

```sh
./mkpackcfg.py -n mypack -p binarypass -o alice secret -u hub1 host.com 1234
./mkpackcfg.py -i -f mypack.cfg
./mkpackcfg.py --dry-run -n foo -p bar -o alice pass -u hub1 h.com 1234
```

Options
-------

Required:

  ``-n, --packname NAME``
    Pack name (no spaces, max 32 chars).
  ``-p, --binarypass PASS``
    Binary password in cleartext (8–64 chars; script computes salted-SHA1).
  ``-o, --owner NICK PASS [HOST...]``
    Owner definition (password 8–64 chars). Repeat for multiple owners.
  ``-u, --hub NICK HOST PORT``
    Hub definition. Repeat for multiple hubs.

Optional:

  ``-d, --dccprefix PREFIX``
    DCC command prefix, exactly one character (default: ``.``).
  ``--salt1 SALT1``
    32-char salt. Auto-generated if omitted.
  ``--salt2 SALT2``
    16-char salt. Auto-generated if omitted.
  ``-f, --output FILE``
    Write to FILE instead of stdout.
  ``-i, --interactive``
    Prompt for missing required fields.
  ``--dry-run``
    Show generated config without writing.
  ``--color {auto,always,never}``
    Colorized output (default: auto).
  ``--version``
    Show version and exit.

Constraints
----------

- **BINARYPASS, OWNER passwords**: 8–64 characters (enforced by this script
  and by wraith >= 1.5 at runtime).
- **DCCPREFIX**: exactly one character (``settings.dcc_prefix[2]`` in C).
- **Hub host**: FQDN or IP. FQDN recommended so the hub can be moved without
  reconfiguring leafs.
- **Pack name**: max 32 chars, no spaces.

Salted-SHA1 Format
------------------

Format: ``+<5-char-salt>$<40-char-SHA1-hex>`` (47 chars total)

The password is hashed as ``SHA1(salt + password)``. This matches
``src/crypt.cc:salted_sha1()``.

Example
-------

```sh
./mkpackcfg.py -n mypack -p secret123 -o alice mypass -o bob bobpass \
               -u hub1 hub.example.com 12742 -u hub2 other.net 27134
```

Output ``pack.cfg``:

```text
PACKNAME mypack
BINARYPASS +aB3xZ$6e8e5b2448356bb48f642dd18115aaaaca7b6dcb
DCCPREFIX .
OWNER alice +xK9mN$d2312f8fcd9de09574d7370e8de058d91322686c
OWNER bob +H19aF$b92a17c87312622a327f5eaea10ec9f8f87a705e
HUB hub1 hub.example.com 12742
HUB hub2 other.net 27134
SALT1 04di1d02b3f3lJFfH6IgTEQxQDWdVkUk
SALT2 looOSwBLxS12XVTn
```

Compatibility
-------------

- Output works with ``build.sh pack.cfg`` and ``./wraith -q pack.cfg``.
- BINARYPASS and OWNER passwords use SHA1, handled by the existing
  ``salted_sha1()`` / ``salted_sha1cmp()`` in ``src/crypt.cc``.
- Wraith >= 1.5 rejects passwords over 64 characters. 1.4.x users migrating
  must ensure all passwords are <=64 before upgrading.
- The user auth system (``+Auth`` via ``makehash()`` in ``src/auth.cc``)
  uses SHA256 independently — no conflict.
