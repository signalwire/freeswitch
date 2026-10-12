# 10. Runbook

<!-- maintained-by: human+ai -->

Ops after a working install. First-time bootstrap, Autotools, and package install live in [Quick Start](01-quick-start.md). Pipelines and prefix layout live in [Build](08-build.md).

Vanilla XML is a **demo PBX**, not a production baseline (`conf/vanilla/README_IMPORTANT.txt`). Change SIP/VM passwords (or run `scripts/perl/randomize-passwords.pl`) and the ESL password before any public IP.

`$PREFIX` below is the configure prefix. Developer path in this PKB: `"$HOME/fs"`. Autotools default without `--prefix`: `/usr/local/freeswitch` (`configure.ac`). Debian packages use FHS paths (see [Inspect Local State](#inspect-local-state)).

## Prerequisites

| Tool | Minimum version | Check command |
|------|-----------------|---------------|
| Installed `freeswitch` | this tree: **1.11.3-dev** | `"$PREFIX/bin/freeswitch" -version` |
| `fs_cli` (ESL client) | same prefix / package | `"$PREFIX/bin/fs_cli" -help` |
| TCP 8021 on loopback | Event Socket | `nc -z 127.0.0.1 8021` once the switch is up |
| SIP/RTP ports (if you will register or place calls) | 5060/5080 UDP/TCP, TLS 5061/5081, RTP UDP 16384–32768 | `ss -ulnp` / `netstat -ulnp` |

Source-build toolchain, Sofia-SIP, and SignalWire PAT are **not** repeated here — see [Quick Start](01-quick-start.md) if `freeswitch` is not on disk yet.

## Setup

If `make install` or a Debian package already landed binaries and config, you are done. Otherwise:

```bash
# source prefix (developer)
# see 01-quick-start.md — bootstrap / configure / make / make install

# Debian packaged daemon
# see 01-quick-start.md — FSGET + apt-get install freeswitch-meta-all
```

Confirm the install layout exists:

```bash
ls "$PREFIX/bin/freeswitch" "$PREFIX/bin/fs_cli" "$PREFIX/conf" "$PREFIX/mod" "$PREFIX/db" "$PREFIX/log" "$PREFIX/run"
```

Debian unit (`debian/freeswitch-systemd.freeswitch.service`) expects `/usr/bin/freeswitch`, config under `/etc/freeswitch`, logs under `/var/log/freeswitch`, state under `/var/lib/freeswitch`.

## Run the Project

### Prefix install (developer)

```bash
"$PREFIX/bin/freeswitch" -ncwait -nonat
"$PREFIX/bin/fs_cli" -x status
```

- `-ncwait`: background, no console, wait until the core is ready (`src/switch.c`).
- `-nonat`: skip UPnP/NAT-PMP pinholes (`conf/vanilla/README_IMPORTANT.txt`).
- Foreground console: `"$PREFIX/bin/freeswitch" -c -nonat`.

**Stop:**

```bash
"$PREFIX/bin/freeswitch" -stop          # reads $PREFIX/run/freeswitch.pid, SIGTERM → elegant shutdown
"$PREFIX/bin/fs_cli" -x shutdown        # API shutdown (SCSC_SHUTDOWN)
# or: kill -TERM "$(cat "$PREFIX/run/freeswitch.pid")"
```

**Reset** (wipe core SQLite; stop first):

```bash
"$PREFIX/bin/freeswitch" -stop
rm -f "$PREFIX/db"/*.db
"$PREFIX/bin/freeswitch" -ncwait -nonat
```

That drops `core.db`, Sofia registration DBs (`sofia_reg_internal.db`, `sofia_reg_external.db`), and other module SQLite files under `$PREFIX/db/` (`src/switch_core_db.c` names them `$dbdir/<dsn>.db`).

### Debian systemd

```bash
sudo systemctl start freeswitch
sudo systemctl status freeswitch
sudo systemctl stop freeswitch
sudo systemctl restart freeswitch
```

Unit start line: `/usr/bin/freeswitch -u freeswitch -g freeswitch -ncwait -nonat`. `PIDFile=/run/freeswitch/freeswitch.pid`. `DAEMON_OPTS=-nonat` unless `/etc/default/freeswitch` overrides it.

### Docker

Containers expect **host networking** (`docker/README.md`):

```bash
docker run --network host …
```

Healthcheck is `fs_cli -x status | grep -q ^UP` (`docker/base_image/healthcheck.sh`).

### Event Socket

`mod_event_socket` must be loaded (`conf/vanilla/autoload_configs/modules.conf.xml`). `fs_cli` defaults (`libs/esl/fs_cli.c`, `libs/esl/fs_cli.conf`): **127.0.0.1:8021**, password **`ClueCon`**.

Module sample binds loopback (`src/mod/event_handlers/mod_event_socket/conf/autoload_configs/event_socket.conf.xml`). **Vanilla runtime config binds `listen-ip` `::`** (`conf/vanilla/autoload_configs/event_socket.conf.xml`) with inbound ACL commented out. Change the password and bind to loopback (or enable `apply-inbound-acl`) before any non-loopback exposure.

## Build and Package

Do not rebuild here. Use:

- [Quick Start](01-quick-start.md) — `./bootstrap.sh -j && ./configure --prefix=… && make && make install`
- [Build](08-build.md) — per-module `make mod_sofia` / `make mod_sofia-install`, Debian FSDEB, Docker, Windows

Runtime load list is **not** `modules.conf`. That file (copied from `build/modules.conf.in` by `bootstrap.sh`) is **build-time**. Runtime load is `$PREFIX/conf/autoload_configs/modules.conf.xml`.

## Inspect Local State

| State | Prefix install (`$PREFIX`) | Debian / FHS | How to inspect |
|-------|----------------------------|--------------|----------------|
| PID | `$PREFIX/run/freeswitch.pid` | `/run/freeswitch/freeswitch.pid` | `cat …/freeswitch.pid`; `ps -p $(cat …)` |
| Process | `freeswitch` | `systemctl status freeswitch` | `ps aux \| grep '[f]reeswitch'` |
| Core SQL | `$PREFIX/db/core.db` | `/var/lib/freeswitch/db/core.db` | `ls -l …/db/`; schema in `src/switch_core_sqldb.c` (DSN `"core"`) |
| Sofia SQL | `$PREFIX/db/sofia_reg_<profile>.db` | same under FHS `db/` | `ls …/db/sofia_reg_*.db`; profiles `internal` / `external` |
| Logs | `$PREFIX/log/freeswitch.log` | `/var/log/freeswitch/freeswitch.log` | `tail -f …/freeswitch.log`; rotate on HUP (`conf/vanilla/autoload_configs/logfile.conf.xml`) |
| Console log | stdout if `-c`; else `mod_console` mappings | journal + logfile | `conf/vanilla/autoload_configs/console.conf.xml`; `fs_cli -x "console loglevel debug"` |
| Config | `$PREFIX/conf/` | `/etc/freeswitch/` | vanilla samples; flattened `freeswitch.xml.fsxml` — do not edit while running |
| Modules (`.so`) | `$PREFIX/mod/` | `${libdir}/freeswitch/mod` | `ls …/mod/mod_sofia*`; `fs_cli -x "module_exists mod_sofia"` |
| ESL | TCP 8021 | same | `ss -lntp \| grep 8021` |

Optional location overrides (same binary): `-conf`, `-log`, `-run`, `-db`, `-mod`, `-base` (`src/switch.c`).

## Verification Commands

```bash
fs_cli -x status                                    # working: a line starting with UP (docker/base_image/healthcheck.sh)
fs_cli -x version
fs_cli -x "module_exists mod_event_socket"          # true
fs_cli -x "module_exists mod_sofia"                 # true
fs_cli -x "sofia status"                            # vanilla internal / external profiles
fs_cli -x "sofia status profile internal"
fs_cli -x "show channels"
fs_cli -x "show calls"
fs_cli -x "show modules"
```

After XML edits: `fs_cli -x reloadxml`. Directory and dialplan changes apply on the **next** call. Sofia bind/codec/gateway XML needs `sofia profile internal rescan` (or restart); ACL lists need `reloadacl`. `reloadxml` does **not** load/unload modules — use `load` / `unload` / `reload mod_name` ([Chapter 5](https://developer.signalwire.com/freeswitch/configuration/module-loading/)).

Vanilla SIP ports (`conf/vanilla/vars.xml`): internal **5060** / TLS **5061**, external **5080** / TLS **5081**. RTP default range **16384–32768** (`src/switch_rtp.c`; commented overrides in `conf/vanilla/autoload_configs/switch.conf.xml`). Docker also documents UDP **64535–65535**.

Directory users **1000–1019** exist under `conf/vanilla/directory/` with shared password `1234` (`$${default_password}` in `vars.xml`) — do not expose 5060/8021 on a public IP. First-call numbers (`9196` echo, `4000` voicemail, …): [Quick Start](01-quick-start.md).

## Debugging Notes

- **CLI / ESL**: `fs_cli` first. If it cannot connect, the process is down, ESL is not on 8021, or `mod_event_socket` did not load. Default client target is loopback (`libs/esl/fs_cli.c`).
- **Runtime / SIP**: `$PREFIX/log/freeswitch.log` and console mappings. SIP trace: `fs_cli -x "sofia profile internal siptrace on"` (keybinding 10 in `conf/vanilla/autoload_configs/switch.conf.xml`). NAT surprises: you started without `-nonat`.
- **Data / persistence**: core scoreboard is SQLite under `$PREFIX/db/` (`src/switch_core_sqldb.c`). `show channels` / `show calls` read that projection; the session thread is source of truth ([Data and API](05-data-and-api.md)). Wipe `*.db` only while stopped.

Log verbosity: `fs_cli -x "console loglevel debug"`; file logger maps `all` to `console,debug,info,notice,warning,err,crit,alert` in vanilla `logfile.conf.xml`. Rotate: `fs_cli -x "fsctl send_sighup"` (`rotate-on-hup` is `true`).

Deeper log/metrics layout: [Observability](11-observability.md).

## Common Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| `fs_cli` cannot connect | Switch not up, ESL not on 8021, or `mod_event_socket` not loaded | `"$PREFIX/bin/freeswitch" -ncwait -nonat`; `module_exists mod_event_socket`; `ss -lntp \| grep 8021` |
| Module missing at runtime but you expected it | **Two different files**: `modules.conf` is **build**; `modules.conf.xml` is **runtime** | Uncomment `src/mod/…` in `modules.conf`, `make mod_<name>` + install; add `<load module="mod_<name>"/>` to `autoload_configs/modules.conf.xml`; `load mod_<name>` or restart |
| `sofia status` empty / no SIP | `mod_sofia` not built or not loaded | `ls "$PREFIX/mod/mod_sofia"*`; `module_exists mod_sofia`; load it or rebuild from [Build](08-build.md) |
| SIP bind fails / profile DOWN | Port 5060/5080 already in use, or wrong `sip-ip` | `ss -ulnp \| grep 506`; check `$${local_ip_v4}` in `conf/vanilla/vars.xml` and `sip_profiles/internal.xml` |
| One-way / no audio | RTP UDP range not open (16384–32768, plus Docker 64535–65535) | Open the range on host/firewall; Docker: `--network host` (`docker/README.md`) |
| Unexpected inbound SIP / “pinhole” | Auto NAT (UPnP/NAT-PMP) | Start with `-nonat` |
| Compromised demo extensions or ESL | Vanilla users 1000–1019; password `ClueCon`; vanilla ESL `listen-ip` `::` | Randomize SIP/VM passwords; change ESL password; bind ESL to loopback / enable ACL |
| `FSGET` / packaged apt 401 | No SignalWire PAT | PAT from `scripts/packaging/README.md`, or build from source ([Quick Start](01-quick-start.md)) |
| Docker SIP/RTP broken with `-p` | Published port ranges vs host net | `docker run --network host` (`docker/README.md`) |
| Config edit has no effect | Edited XML not reloaded; or Sofia needs profile restart | `reloadxml`; for bind/listen changes restart the profile or the process. Do not edit `freeswitch.xml.fsxml` live |
| `Cannot open pid file` on `-stop` | Process already dead, or different `-run` / prefix | Confirm `$PREFIX/run/freeswitch.pid` vs `/run/freeswitch/freeswitch.pid` |
| SQLite errors after crash | Stale `$PREFIX/db/*.db` | Stop, move/remove `*.db`, start again (see Reset) |

## Related Documentation

- [Quick Start](01-quick-start.md)
- [Build](08-build.md)
- [Testing](09-testing.md)
- [Observability](11-observability.md)
- [Data and API](05-data-and-api.md)
- [Architecture](02-architecture.md)

Operator configuration: [FreeSWITCH Users Manual](https://developer.signalwire.com/freeswitch/) ([Ch 2](https://developer.signalwire.com/freeswitch/foundations/getting-started), Part 11 Troubleshooting). Historical wiki: [FreeSWITCH Explained](https://developer.signalwire.com/freeswitch/FreeSWITCH-Explained/).

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
