# Appendix-01: FAQ

<!-- maintained-by: human+ai -->

Answers below come from [Quick Start](01-quick-start.md), [Architecture](02-architecture.md), [Build](08-build.md), [Data and API](05-data-and-api.md), the [Users Manual](https://developer.signalwire.com/freeswitch/), and `README.md`. Vanilla XML is a demo PBX, not a production baseline.

## Frequently Asked Questions

### Should I install from source or from packages?

`README.md` recommends **packages** for operators. That path is FSGET plus `freeswitch-meta-all` (`scripts/packaging/README.md`). FSGET needs a SignalWire Personal Access Token; without it, `apt` / `docker/master` returns **401**.

This clone is the **source** path: install Sofia-SIP / libks / SpanDSP / signalwire-c first, then `./bootstrap.sh -j && ./configure --prefix="$HOME/fs" && make && make install`. `release` tracks packaged releases; `prerelease` tracks `master`.

### Why are Sofia-SIP, SpanDSP, and libks missing from this git tree?

They are **out-of-tree**. CI clones [sofia-sip](https://github.com/freeswitch/sofia-sip); `docker/examples/Debian11/Dockerfile` also builds libks, SpanDSP, and signalwire-c. macOS CI uses Homebrew `signalwire/homebrew-signalwire/{libks2,signalwire-c2,spandsp}`. `configure` / link fails until those libraries are installed. They are not under `libs/` (APR, SRTP, ESL, VPX *are*).

### Why are there two files named `modules.conf`?

| File | When | Role |
|------|------|------|
| `build/modules.conf.in` → `modules.conf` | Build | Which `src/mod/...` trees `bootstrap.sh` / Autotools compile |
| `conf/vanilla/autoload_configs/modules.conf.xml` | Runtime | Which compiled DSOs `switch_loadable_module_init()` loads |

`bootstrap.sh` copies `modules.conf.in` → `modules.conf` if missing. Uncomment a `src/mod/...` line **after** bootstrap and **before** `./configure` / `make`. A module can be compiled and still stay unloaded if it is absent from the XML list.

### What is the default prefix, and how do I start FreeSWITCH?

Default prefix without `--prefix` is `/usr/local/freeswitch` (`configure.ac`). A home prefix (`--prefix="$HOME/fs"`) avoids root and matches macOS CI.

```bash
"$HOME/fs/bin/freeswitch" -ncwait -nonat
```

`-ncwait`: background, no console, wait until the core is ready (`src/switch.c`). `-nonat`: skip UPnP/NAT-PMP pinholes — required unless you *want* auto-NAT (`conf/vanilla/README_IMPORTANT.txt`). Foreground: `freeswitch -c -nonat`. Stop: `freeswitch -stop`. Debian systemd uses `/usr/bin/freeswitch -u freeswitch -g freeswitch -ncwait -nonat`.

### How do I prove FreeSWITCH is up?

```bash
"$HOME/fs/bin/fs_cli" -x status
```

Working output starts with `UP` (same check as `docker/base_image/healthcheck.sh`, regex `^UP`). Then `fs_cli -x sofia status` for SIP profiles. If `fs_cli` cannot connect, the switch is not up or ESL is not on loopback 8021 (`mod_event_socket` not loaded).

First call without a second phone: register as `1000` / password `1234` on port 5060, then dial **`9196`** (echo). See [Quick Start](01-quick-start.md) and [Users Manual Chapter 2](https://developer.signalwire.com/freeswitch/foundations/getting-started).

### Is vanilla XML a production config?

No. `conf/vanilla` is a **demo PBX** so you can test immediately (`conf/vanilla/README_IMPORTANT.txt`). Directory users `1000`–`1019` share **`default_password=1234`** (`conf/vanilla/vars.xml`). Do not publish 5060/8021 without ACLs. Before any public bind: change `default_password` (or `scripts/perl/randomize-passwords.pl`), change the ESL password, and start with `-nonat` unless NAT helpers are intentional.

### What is the default Event Socket port and password?

Password **`ClueCon`**, port **8021**. Distinguish three files:

- Vanilla runtime (`conf/vanilla/autoload_configs/event_socket.conf.xml`) listens on **`::`** (all interfaces) with inbound ACL commented out.
- Module sample (`src/mod/event_handlers/mod_event_socket/conf/autoload_configs/event_socket.conf.xml`) binds **`127.0.0.1`**.
- `fs_cli` client default is **`127.0.0.1:8021`** (`libs/esl/fs_cli.c`, `libs/esl/fs_cli.conf`).

Change the password and bind/ACL before any non-loopback exposure. Wrong password closes the socket (`-ERR invalid`).

### Why does Docker SIP or RTP fail unless I use host networking?

Runtime containers expect **`--network host`** (`docker/README.md`). Typical ports: SIP 5060/5080, TLS 5061/5081, WebSocket 5066/7443, ESL **8021**, RTP UDP 16384–32768 and 64535–65535. Publishing those UDP ranges through a user-defined bridge is the usual failure mode. Packaged images (`docker/master/Dockerfile`) still need a SignalWire `TOKEN` build-arg; a token-free source image is `docker/examples/Debian11/Dockerfile`.

### How do I build on Windows vs Unix?

Unix / macOS: Autotools — `./bootstrap.sh -j && ./configure --prefix=... && make && make install`. Windows: `Freeswitch.2017.sln` / `w32/`, typically `msbuild Freeswitch.2017.sln -t:build -verbosity:minimal -property:Configuration=Release -property:Platform=x64` (`msbuild.cmd` locates VS via `vswhere.exe`). CI uploads `x64\*.msi` on `master` / `v1.10` / `v1.11`. Historical `src/CMakeLists.txt` is **not** the Unix CI path.

### Where are the prompt / sound files?

They are **not** in this git tree. `make sounds` / `make sounds-install` downloads Callie 8 kHz packages (`Makefile.am`, default `en-us-callie-8000`). Specs at repo root: `freeswitch-sounds-*.spec`. Windows sound packages live in a separate [freeswitch-sounds](https://github.com/freeswitch/freeswitch-sounds) release repo (`README.md`).

### What is the difference between `$${var}` and `${var}`?

`$${name}` is a **preprocessor** variable, expanded once while assembling XML (`vars.xml`, `#set` / `X-PRE-PROCESS cmd="set"`). `${name}` is a **channel** variable, expanded at call time. Mixing them is a common config bug. [Users Manual Chapter 3](https://developer.signalwire.com/freeswitch/configuration/xml).

### Why does my internal SIP profile say `context=public` but registered phones use the `default` dialplan?

Vanilla `sip_profiles/internal.xml` sets `context=public` for **unauthenticated** inbound. After digest auth, the directory user’s `user_context` (vanilla `default`) wins. [Chapter 1](https://developer.signalwire.com/freeswitch/foundations/introduction).

### Where is this Project Knowledge Base?

**`man/`** at the repo root — not `docs/man/` (Unix man pages). Sphinx: `cd man && poetry install && make html-en`. Operator configuration is the [FreeSWITCH Users Manual](https://developer.signalwire.com/freeswitch/). Historical tutorials remain on [FreeSWITCH Explained](https://developer.signalwire.com/freeswitch/FreeSWITCH-Explained/) and [Confluence](https://freeswitch.org/confluence/). The PKB maps those concepts onto files in this clone; it does not copy parameter tables.

### Why does this tree say `1.11.3-dev` when CI also builds `v1.10` / `v1.11`?

This workspace is **1.11.3-dev** (`AC_INIT` in `configure.ac`; keep in sync with `SWITCH_VERSION_*` and `build/next-release.txt`). GitHub Actions run on `master`, `v1.10`, and `v1.11`. Packaged `build.yml`: those version branches → `release`; `master` → **unstable**. FSGET `prerelease` tracks what you see on `master`. Verify a running binary with `freeswitch -version` / `fs_cli -x version`.

## Related Documentation

- [Quick Start](01-quick-start.md)
- [Architecture](02-architecture.md)
- [Build](08-build.md)
- [Runbook](10-runbook.md)
- [Documentation Process](12-document.md)
- [Glossary](appendix-02-glossary.md)
- [Users Manual](https://developer.signalwire.com/freeswitch/)

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
