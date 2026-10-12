# 01. Quick Start

<!-- maintained-by: human+ai -->

Goal: get a `freeswitch` process running from **this clone** and prove it with `fs_cli -x status`. Vanilla XML is a demo PBX, not a production baseline (`conf/vanilla/README_IMPORTANT.txt`). Operator-facing first-call steps (register 1000/1001, dial `9196`) match [Users Manual Chapter 2](https://developer.signalwire.com/freeswitch/foundations/getting-started).

If you only need a packaged daemon on Debian and have a SignalWire token, skip to [Install from packages](#install-from-packages-debian). That is what `README.md` recommends for operators.

## Prerequisites

| Tool | Minimum version | Check command | Install notes |
|------|-----------------|---------------|---------------|
| Git | any recent | `git --version` | Clone this tree |
| autoconf | 2.59+ | `autoconf --version` | Enforced by `scripts/ci/build-requirements.sh` via `bootstrap.sh` |
| automake / aclocal | 1.7+ | `automake --version` | Same checker |
| libtool / libtoolize | 1.5.14+ (2.x is fine) | `libtool --version` | macOS: Homebrew `libtool` (`glibtool`) |
| C/C++ toolchain | GCC or Clang | `cc --version` | `build-essential` on Debian; Xcode CLT on macOS |
| pkg-config | any | `pkg-config --version` | Needed by `configure` |
| Sofia-SIP, libks, SpanDSP, signalwire-c | matching current FreeSWITCH | `pkg-config --exists sofia-sip-ua` | **Not fully in this repo.** CI clones [sofia-sip](https://github.com/freeswitch/sofia-sip); the Debian 11 source Dockerfile also builds libks, spandsp, and signalwire-c (`docker/examples/Debian11/Dockerfile`). |

Debian/Ubuntu source-build packages used in `docker/examples/Debian11/Dockerfile` include `build-essential`, `cmake`, `automake`, `autoconf`, `libtool-bin`, `pkg-config`, `libssl-dev`, `zlib1g-dev`, `libpcre2-dev`, `libedit-dev`, `libsqlite3-dev`, `libcurl4-openssl-dev`, `libspeexdsp-dev`, `libopus-dev`, `libsndfile1-dev`, `liblua5.2-dev`, `python3-dev`, `libpq-dev`, and related codec/AV headers.

macOS CI (`.github/workflows/macos.yml`) installs Homebrew formulas (`autoconf`, `automake`, `libtool`, `pcre2`, `sofia-sip`, `opus`, `speexdsp`, …) plus `signalwire/homebrew-signalwire/{libks2,signalwire-c2,spandsp}`.

## Clone and Install

You already have the tree if you are reading this PKB. From a fresh machine:

```bash
git clone https://github.com/signalwire/freeswitch.git
cd freeswitch
```

This workspace is version **1.11.3-dev** (`configure.ac`). Upstream default remote is `https://github.com/signalwire/freeswitch`.

### Source build (developer path)

Install Sofia-SIP (and libks / SpanDSP / signalwire-c) first, the same way CI and `docker/examples/Debian11/Dockerfile` do. Then, from the repo root:

```bash
./bootstrap.sh -j
./configure --prefix="$HOME/fs"
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
make install
```

- `bootstrap.sh` copies `build/modules.conf.in` → `modules.conf` if missing, then runs Autotools. `-j` parallelizes library bootstraps.
- Default prefix without `--prefix` is `/usr/local/freeswitch` (`configure.ac`). A home prefix avoids root and matches the macOS CI pattern (`--prefix=.../OUT`).
- `make install` also installs vanilla sample config when `$(confdir)` does not exist (`Makefile.am`: `test -d $(DESTDIR)$(confdir) || $(MAKE) samples-conf`).
- Prompts and music-on-hold are **not** in this git tree. After `make install`, [Chapter 2](https://developer.signalwire.com/freeswitch/foundations/getting-started) also runs `make cd-sounds-install cd-moh-install` (Callie 8 kHz packages; `Makefile.am` `sounds-install`). Skip only if you do not need playback/echo tests.
- Debug symbols without changing configure flags: `./devel-bootstrap.sh` sets `CFLAGS`/`CXXFLAGS` to `-ggdb3 -O0` then bootstraps and configures.

Enable extra modules by uncommenting lines in `modules.conf` **after** bootstrap and **before** `./configure` / `make`. CI’s unit-test configure (`ci.sh`) is an example of that edit pattern.

### Install from packages (Debian)

Requires a SignalWire Personal Access Token. From `scripts/packaging/README.md`:

```bash
apt update && apt install -y curl
curl -sSL https://freeswitch.org/fsget | bash -s <PAT or API token> [release|prerelease] [install]
apt-get install -y freeswitch-meta-all
```

`release` tracks packaged releases; `prerelease` tracks what you see on `master`.

Meta packages (Users Manual Ch 2): `freeswitch-meta-all` (every module), `freeswitch-meta-vanilla` (vanilla set + typically `freeswitch-sounds-en-us-callie` / `freeswitch-sounds-music`), `freeswitch-meta-default`, `freeswitch-meta-bare`. Packaged config root is `/etc/freeswitch`; systemd: `systemctl enable --now freeswitch`.

### Docker

Packaged image (`docker/master/Dockerfile`) also needs a SignalWire `TOKEN` build-arg. For a **source** image without that token, follow `docker/examples/Debian11/Dockerfile` (clone deps, `./bootstrap.sh -j`, `./configure`, `make && make install`).

Runtime containers expect **host networking** (`docker/README.md`). Typical ports: SIP 5060/5080, TLS 5061/5081, WebSocket 5066/7443, ESL **8021**, RTP UDP ranges 16384–32768 and 64535–65535.

## Run Locally

After a prefix install:

```bash
"$HOME/fs/bin/freeswitch" -ncwait -nonat
"$HOME/fs/bin/fs_cli" -x status
```

- `-ncwait`: background, no console, wait until the core is ready (`src/switch.c`).
- `-nonat`: skip UPnP/NAT-PMP pinholes — required unless you *want* auto-NAT (`conf/vanilla/README_IMPORTANT.txt`).
- Foreground console instead: `freeswitch -c -nonat`.
- Stop: `freeswitch -stop` (same prefix `bin/`).

Default Event Socket is `127.0.0.1:8021` with password `ClueCon` (`src/mod/event_handlers/mod_event_socket/conf/autoload_configs/event_socket.conf.xml`, `libs/esl/fs_cli.conf`). `fs_cli` uses that by default.

The **configuration root** is the directory that contains `freeswitch.xml` ([Chapter 2](https://developer.signalwire.com/freeswitch/foundations/getting-started)):

| Install type | Typical configuration root |
|--------------|----------------------------|
| Source build (this PKB, `--prefix="$HOME/fs"`) | `$HOME/fs/conf` |
| Source build (Autotools default prefix) | `/usr/local/freeswitch/conf` |
| Debian package | `/etc/freeswitch` |

**Working looks like:**

1. `fs_cli -x status` prints a line starting with `UP` (same check as `docker/base_image/healthcheck.sh`).
2. `fs_cli -x sofia status` shows SIP profiles (vanilla internal/external).
3. Directory users `1000`–`1019` exist in `conf/vanilla/directory/` with shared password `$${default_password}` = **`1234`** (`conf/vanilla/vars.xml`). Do not expose them on a public IP.

### First call (vanilla PBX)

Optional, after the process is `UP`. Matches Users Manual Chapter 2 against this tree’s `conf/vanilla/dialplan/default.xml`.

1. Register a SIP softphone as user `1000` (password `1234`, port **5060**, UDP or TCP) to the host’s SIP address. A second client as `1001` lets you call between extensions.
2. Confirm bindings: `fs_cli -x "show registrations"` and `fs_cli -x "sofia status profile internal reg"`.
3. From `1000`, dial **`1001`** (`Local_Extension` `^(10[01][0-9])$`) or dial **`9196`** for the echo test (no second device).

| Number | Destination in vanilla `default` context |
|--------|------------------------------------------|
| `1000`–`1019` | Registered test extensions |
| `9196` | Echo (`echo` app) |
| `9195` | Delayed echo (`delay_echo` **5000** ms in this tree — not 250 ms) |
| `9197` | Milliwatt tone (1004 Hz) |
| `9198` | Tone stream demo |
| `9664` | Music on hold |
| `5000` | IVR demo |
| `5001` | Dynamic conference |
| `3000`–`3099` | Named conferences (`30xx` narrowband) |
| `4000` or `*98` | Voicemail main (`vmain`) |
| `0` or `operator` | Operator (transfers to 1000 XML features) |

Change `default_password` in `vars.xml` before any untrusted network, then `reloadxml`. `fs_cli` number keys 1–12 map to help/status/sofia/reloadxml/siptrace (`conf/vanilla/autoload_configs/switch.conf.xml`).

## Run Tests

Unit tests are Autotools programs under `tests/unit/` linking `libfreeswitch`. CI installs first, then:

```bash
# after configure + make + make install (CI path)
cd tests/unit
./run-tests.sh                  # all tests
./run-tests.sh 4 1              # chunk 1 of 4, as in GitHub Actions
make -C ../../libs/esl check    # ESL tests; CI runs this on group 1
```

`./run-tests.sh` calls `make -C ../.. print_tests`, then `make -f run-tests.mk`. New tests must use `src/include/test/switch_test.h` (`tests/unit/README`).

ASAN unit-test configure (from `ci.sh -t unit-test`):

```bash
./ci.sh -t unit-test -a configure -c freeswitch
./ci.sh -t unit-test -a build -c freeswitch
./ci.sh -t unit-test -a install -c freeswitch
```

Sofia-SIP must already be built/installed for that flow (see `.github/workflows/unit-test.yml`).

## Build for Production

| Goal | Command / path |
|------|----------------|
| Unix prefix install | `./bootstrap.sh -j && ./configure --prefix=... && make && make install` |
| Debian packages from a **git** tree | `scripts/packaging/build/README.md` (`FSDEB`); tarball checkouts are not supported |
| Docker (packages) | `docker/master/Dockerfile` with `TOKEN` |
| Windows | `Freeswitch.2017.sln` / `w32/` |

Install layout under `--prefix` (defaults in `configure.ac`): `bin/`, `mod/`, `conf/`, `log/`, `db/`, `scripts/`, `htdocs/`, `sounds/`.

Before any public deployment: change SIP and voicemail passwords (or run `scripts/perl/randomize-passwords.pl`), change the ESL password, and start with `-nonat` unless NAT helpers are intentional.

## Common Issues

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `bootstrap.sh` exits on autoconf/automake/libtool | Tool versions below `scripts/ci/build-requirements.sh` | Install GNU autotools; on macOS use Homebrew `autoconf` `automake` `libtool` |
| `configure` / link fails on Sofia, KS, SpanDSP, or signalwire | Those libraries are out-of-tree | Build them first as in `docker/examples/Debian11/Dockerfile` or `.github/workflows/unit-test.yml` / `macos.yml` |
| `modules.conf` missing modules you expected | File is a copy of `build/modules.conf.in`; commented lines are disabled | Uncomment the `src/mod/...` path, then rebuild that module (`make mod_lua` style targets in top-level `Makefile.am`) |
| `fs_cli` cannot connect | Switch not up, or ESL not on 8021 | `freeswitch -ncwait`; confirm `mod_event_socket` loaded; default bind is loopback |
| Router “pinhole” / unexpected inbound SIP | Auto NAT (UPnP/NATPMP) | Start with `-nonat` |
| Compromised demo extensions | Vanilla users 1000–1019 and ESL `ClueCon` | Randomize passwords; do not publish 5060/8021 without ACLs |
| `FSGET` / `docker/master` apt repo 401 | No SignalWire PAT | Create a PAT as linked from `scripts/packaging/README.md`, or build from source |
| Docker RTP/SIP broken | Published port ranges vs host net | Use `docker run --network host` (`docker/README.md`) |
| `debian_min_build.sh` fails on modern Debian | Script pins **Jessie / FreeSWITCH 1.6** repos | Use the Debian 11 Dockerfile or current FSGET docs, not that helper |

## Next Steps

- [Overview](00-overview.md)
- [Repository Map](04-repo-map.md)
- [Architecture](02-architecture.md)
- [Tech Stack](03-tech-stack.md)
- [Build](08-build.md)
- [Runbook](10-runbook.md)

Operator configuration: [FreeSWITCH Users Manual](https://developer.signalwire.com/freeswitch/) — especially [Chapter 2 Getting Started](https://developer.signalwire.com/freeswitch/foundations/getting-started). Historical wiki: [FreeSWITCH Explained](https://developer.signalwire.com/freeswitch/FreeSWITCH-Explained/).

---
<!-- PKB-metadata
last_updated: 2026-08-17
commit: d94936cc10
updated_by: human+ai
review_status: pending
review_score: 0
reviewed_by:
confidentiality: L1
-->
