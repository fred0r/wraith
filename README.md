# Wraith

[![Build Status](https://travis-ci.org/wraith/wraith.png?branch=master)](https://travis-ci.org/wraith/wraith)

* https://github.com/wraith/wraith/wiki
* http://github.com/wraith/wraith
* @wraithbotpack

Wraith is an IRC channel management bot written purely in C/C++.
It has been in development since late 2003. It is based on
Eggdrop 1.6.12 but has since evolved into something much
different at its core. TCL and loadable modules are currently
not supported.

* Wraith aims to be a secure and easy to setup and manage botnet.
* A botnet can be setup in a matter of minutes and updated later with 1 command.
* Leaf bots save no files locally, but rather store configuration encrypted in
  their own binary.
* Hubs do not connect to IRC, and keep a local encrypted copy of the userfile.

All official sites, documentation, support venues, repositories and source urls
are referenced here.

For official release announcements send an email to:
  wraith-announce-subscribe@botpack.net

Download: https://github.com/wraith/wraith/wiki/Download
Git: git://github.com/wraith/wraith.git

See git for a list of Contributors: git shortlog -sen master

Support:
* How To Contribute: http://github.com/wraith/wraith/blob/master/CONTRIBUTING.md
* Getting Started: http://github.com/wraith/wraith/wiki/GettingStarted
* FAQ: https://github.com/wraith/wraith/wiki/FrequentlyAskedQuestions
* Documentation Index: https://github.com/wraith/wraith/wiki/Documentation
* Issues can be reported at: https://github.com/wraith/wraith/issues
* #wraith @ EFnet

Please support wraith by signing up for a shell at http://www.xzibition.com (coupon 'wraith' for 30% off)

The botpack ghost inspired the early versions of wraith and a few cmds.
  einride
  ievil

The following botpacks gave inspiration, ideas, and some code:
  awptic by lordoptic
  optikz by ryguy and lordoptic
  celdrop by excelsior
  genocide by Crazi, Dor, psychoid, and Ace24
  tfbot by warknite and loslinux

<details>
<summary><strong>Building</strong></summary>

### Requirements

- A C++11 compiler (`clang++` or `g++`)
- OpenSSL headers and `libcrypto`
- GNU make (`gmake` on FreeBSD/OpenBSD/NetBSD)
- Git submodules when building from a source checkout
- Optional system libelf (`libelf-dev` on Debian/Ubuntu,
  `elfutils-libelf-devel` on Fedora/RHEL); otherwise Wraith builds its
  bundled libelf, which requires zlib headers
- Tests: `pkg-config` and CppUnit >= 1.14
- Pack generation and integration tests: Python 3

```sh
# Debian/Ubuntu
sudo apt install build-essential libssl-dev
sudo apt install libelf-dev                          # optional: system libelf
sudo apt install zlib1g-dev                          # only for bundled fallback
sudo apt install pkg-config libcppunit-dev python3   # tests and pack generator

# FreeBSD
sudo pkg install gmake openssl
sudo pkg install pkgconf cppunit python3             # tests and pack generator
```

If OpenSSL is installed outside the default search paths, tell configure
where to find it:

```sh
./configure --with-openssl=/usr/local
```

### Select libelf

By default configure uses a system libelf when available and falls back to the
bundled copy. Force a mode when needed:

```sh
./configure --with-libelf=auto     # system if usable, otherwise bundled
./configure --with-libelf=system   # require system libelf
./configure --with-libelf=bundled  # always use bundled libelf
```

The bundled copy requires zlib headers. Run `make clean` before switching
libelf modes.

### Build from source

```sh
git submodule update --init --recursive
./configure

# Linux
make -j"$(nproc)"

# FreeBSD/OpenBSD/NetBSD
gmake -j"$(sysctl -n hw.ncpu)"
```

For the bundled FreeBSD/NetBSD build:

```sh
./configure --with-libelf=bundled
gmake -j"$(sysctl -n hw.ncpu)"
```

The default target is `dynamic`. Use `make debug` (or `gmake debug`) for a
debug build. If `./configure` reports missing autotools auxiliary files, run
`./bootstrap.sh` first; it requires `automake`.

### Create a pack and initialize the binary

A pack file defines the pack name, passwords, and hub connection details. A
directly built `./wraith` is uninitialized until it is given one:

```sh
python3 scripts/mkpackcfg/mkpackcfg.py \
  -n mypack \
  -p '<binary-password>' \
  -o owner '<owner-password>' \
  -u hub hub.example.com 12742 \
  -f mypack.cfg

./wraith -q mypack.cfg
```

`-q` initializes the binary once. Keep `mypack.cfg` private: it contains
password hashes and crypto salts.

> **Every bot on the same botnet must use the same `pack.cfg`, but the binary
> must be compiled separately for each platform.**
> Generate `pack.cfg` once, then use that exact file when building and
> initializing every hub and leaf. Build a native binary for Linux, FreeBSD,
> NetBSD, or another target OS as needed. Do not generate a separate pack per
> bot or per platform. Bots initialized with different packs will not link as
> one net.

| Option | Meaning |
| --- | --- |
| `-n`, `--packname` | Pack name embedded in the binary. No spaces; max 32 characters. |
| `-p`, `--binarypass` | Binary password, 8-64 characters. It is stored as a salted SHA-1 hash and is required when the packed bot starts. This is not the owner password. |
| `-o`, `--owner NICK PASS [HOST...]` | Initial owner account. `NICK` is the partyline handle; `PASS` is 8-64 characters. Optional hostmasks restrict the account. Repeat for multiple owners; at least one is required. |
| `-u`, `--hub NICK HOST PORT` | Botnet hub: hub bot nick, hostname or IP, and the hub's botnet port (1-65535). This is not an IRC server. Repeat for multiple or fallback hubs; at least one is required. An FQDN is recommended. |
| `-f`, `--output FILE` | Write the generated pack to `FILE`. |
| `-i`, `--interactive` | Prompt for values instead of placing passwords on the command line. |
| `--salt1`, `--salt2` | Leave unset to generate random salts; only override to reproduce an existing pack. |

### Build and package with `build.sh`

```sh
./build.sh mypack.cfg
./build.sh -d mypack.cfg     # debug build
./build.sh -n mypack.cfg     # build without packaging
```

`build.sh` requires an existing pack file; it does not generate one. It runs
`./configure`, selects `gmake` on BSD, builds the dynamic or debug binary,
initializes a versioned copy with the pack, and packages it unless `-n` is
used.

### Tests

```sh
gmake test                 # unit tests
gmake test-ubsan           # UBSan only
gmake test-asan            # AddressSanitizer + UBSan
gmake test-integration     # fake IRC, linked-bot, and cookie tests
```

Use `make` instead of `gmake` on Linux.

The optional old/new cookie interoperability gate needs a binary built before
the cookie codec refactor:

```sh
tests/integration/build_legacy_wraith.sh /tmp/wraith-old
WRAITH_OLD_BINARY=/tmp/wraith-old/wraith make test-integration-crossversion
```

The script builds commit `2d2b3bfb`. On systems where `make` is BSD make, run
it with `MAKE=gmake`.

- `make test` and `make test-ubsan` require CppUnit >= 1.14 and `pkg-config`.
- `make test-integration` requires Python 3 and must not run as root.
- `make test-integration-crossversion` additionally requires
  `WRAITH_OLD_BINARY`.
- `make test-ubsan` runs UBSan without ASan shadow memory and works on
  restricted hosts.
- `make test-asan` requires unlimited virtual-memory and data-size limits.
  FreeBSD does not support LeakSanitizer, so leak detection is enabled only
  on Linux. Use `test-ubsan` or Valgrind when those limits cannot be raised.

### Cleaning

```sh
make clean       # or gmake clean
make distclean   # removes generated Makefiles/config; rerun ./configure
```

</details>
