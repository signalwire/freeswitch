# Appendix-02: Glossary

<!-- maintained-by: human+ai -->

Terms humans and AI commonly mix up in this tree. Prefer the **Notes** path over guessing.

## Terms

| Term | Meaning | Notes |
|------|---------|-------|
| `-nonat` | Start flag: skip UPnP / NAT-PMP pinholes | Required unless you *want* auto-NAT. Vanilla warning: `conf/vanilla/README_IMPORTANT.txt`. Debian unit: `debian/freeswitch-systemd.freeswitch.service`. |
| `$${name}` | Preprocessor variable, expanded while assembling XML | Defined with `#set` / `X-PRE-PROCESS cmd="set"` in `vars.xml`. Distinct from `${name}` (channel, call time). [Users Manual Ch 3](https://developer.signalwire.com/freeswitch/configuration/xml). |
| `${name}` | Channel variable, expanded at call time | Set by directory, dialplan, and apps. Catalog is Users Manual Ch 32. |
| `CS_*` | `switch_channel_state_t` values on a channel | Default inbound: `CS_NEW` → `CS_INIT` → `CS_ROUTING` → `CS_EXECUTE` → `CS_EXCHANGE_MEDIA` or hangup `CS_HANGUP` → `CS_REPORTING` → `CS_DESTROY`. Enum: `src/include/switch_types.h`. Not the same as callstate `CCS_*`. |
| ClueCon | Default ESL password | Demo value — change before public bind. Vanilla listen is `::`:8021; `fs_cli` client default is `127.0.0.1:8021`. `conf/vanilla/autoload_configs/event_socket.conf.xml`, `libs/esl/fs_cli.conf`. |
| bridge | Association that joins two channels so media flows between them | Dialplan app `bridge`; core `switch_ivr_bridge`. Users Manual Ch 1 term. |
| call | Pair of legs in core SQL | Table `calls`: `call_uuid`, `caller_uuid`, `callee_uuid` (`src/switch_core_sqldb.c`). Not one session. Users Manual: one or more associated channels. |
| channel | One call leg’s state, variables, hangup cause | `switch_channel_t` inside a session. Dialplan/apps key off channel state. `src/switch_channel.c`. Users Manual: one leg between FreeSWITCH and a single endpoint. |
| configuration root | Directory that contains `freeswitch.xml` | Source default: `/usr/local/freeswitch/conf`. Debian: `/etc/freeswitch`. Users Manual paths are relative to this root. |
| dialplan | XML (or other) routing: match extension, run apps | Vanilla section in `conf/vanilla/freeswitch.xml`; default context `conf/vanilla/dialplan/default.xml`. Hunt in `CS_ROUTING` via `mod_dialplan_xml`. [Ch 12](https://developer.signalwire.com/freeswitch/dialplan/xml). |
| directory | XML users / domains (SIP digest, VM, channel vars) | Vanilla `conf/vanilla/directory/` (`1000.xml` …). Directory users are **not** OS users. [Ch 6](https://developer.signalwire.com/freeswitch/users-and-endpoints/user-directory). |
| endpoint | Protocol interface that originates or receives calls | A module: Sofia (`mod_sofia`) for SIP, Verto (`mod_verto`) for WebRTC. Users Manual Ch 1. |
| ESL / Event Socket | Out-of-process TCP control + event subscribe | `mod_event_socket` listens; clients `auth` then `api` / `bgapi` / `event` / `sendmsg`. Vanilla listen `::`:8021; `fs_cli` connects to `127.0.0.1:8021`. Distinct from the in-process event bus (`src/switch_event.c`). [Ch 46](https://developer.signalwire.com/freeswitch/programming/esl-inbound). |
| FreeSWITCH | Modular C softswitch: sessions, media, dialplan, loadable DSOs | This repo’s product. Version **1.11.3-dev** (`configure.ac`). Sponsor SignalWire; hosted cloud is a *separate* product. Operator manual: [Users Manual](https://developer.signalwire.com/freeswitch/). |
| FSDEB | Build Debian packages from a **git** tree | `curl …/fsdeb \| bash -s -- -b BUILD_NUMBER -o OUT_DIR -w /path/to/freeswitch`. Tarball checkouts are not supported. `scripts/packaging/build/README.md`. |
| FSGET | Install packaged FreeSWITCH from SignalWire apt | `curl -sSL https://freeswitch.org/fsget \| bash -s <PAT> [release\|prerelease] [install]`. Needs a PAT. `scripts/packaging/README.md`, `README.md`. |
| `fs_cli` | ESL client shipped with the tree | Built from `libs/esl/fs_cli.c`. Health: `fs_cli -x status` must match `^UP`. |
| libks | KS runtime (libks / libks2), **not** in this git tree | Required if `mod_verto` or `mod_signalwire` is enabled (`configure.ac`). Often confused with bundled `libs/`. |
| `mod_sofia` | SIP endpoint module | Profiles in `conf/vanilla/sip_profiles/` (internal auth vs external `auth-calls=false`). Uses out-of-tree Sofia-SIP. `src/mod/endpoints/mod_sofia/`. [Ch 7](https://developer.signalwire.com/freeswitch/users-and-endpoints/sip-profiles). |
| `modules.conf` | **Build-time** list of `src/mod/...` trees to compile | Copied from `build/modules.conf.in` by `bootstrap.sh`. Commented lines are not built. |
| `modules.conf.xml` | **Runtime** autoload list of compiled modules | `conf/vanilla/autoload_configs/modules.conf.xml`. Loader: `src/switch_loadable_module.c`. Companions: `pre_load_modules.conf.xml`, `post_load_modules.conf.xml`. [Ch 5](https://developer.signalwire.com/freeswitch/configuration/module-loading/). |
| PKB | Project Knowledge Base (this Sphinx/MyST doc set) | Lives in **`man/`**, not `docs/man/`. Tooling: `man/pyproject.toml`. Not linked into the `freeswitch` binary. Does not replace the Users Manual. |
| prefix | Autotools install root | Default `/usr/local/freeswitch` (`AC_PREFIX_DEFAULT` in `configure.ac`). Layout: `bin/`, `mod/`, `conf/`, `log/`, `db/`, `scripts/`, `htdocs/`, `sounds/`. |
| RTP | Real-time media path (audio/video over UDP) | Core: `src/switch_rtp.c`, `src/switch_core_media.c`. Docker needs host networking for the UDP ranges. Media modes: [Ch 17](https://developer.signalwire.com/freeswitch/media-and-codecs/handling). |
| session | One call leg’s thread, codecs, queues, UUID | `switch_core_session_t` wraps a channel. One session thread owns the state machine. `src/switch_core_session.c`. Users Manual: runtime container for a channel and its application state. |
| Sofia-SIP | External SIP stack (`sofia-sip-ua >= 1.13.18`) | **Not** vendored under `libs/`. CI clones `freeswitch/sofia-sip`. Used by `mod_sofia`. |
| SpanDSP | External DSP / fax / codec library (`>= 3.1.1`) | **Not** in this git tree. Hard configure error if missing. `mod_spandsp`. |
| `switch_status_t` | Core C return enum (`SWITCH_STATUS_SUCCESS`, `SWITCH_STATUS_FALSE`, …) | `src/include/switch_types.h`. Endpoints may return `SWITCH_STATUS_FALSE` from a state handler to skip the core’s standard handler. Hangup *cause* is `switch_call_cause_t`, not this type. |
| `user_context` | Directory variable that selects the dialplan context after auth | Vanilla users 1000–1019: `default`. Overrides the SIP profile’s `context` (`public` on internal). |
| Users Manual | Operator configuration reference from SignalWire | [developer.signalwire.com/freeswitch](https://developer.signalwire.com/freeswitch/). Parts 1–8 configuration; 9 modules; 10 recipes; 11 troubleshooting; 12 ESL/scripting. Distinct from this PKB and from Explained/Confluence. |
| vanilla | Default sample XML profile (`conf/vanilla`) | Demo PBX, not a production security baseline. Other profiles: `conf/sbc`, `conf/minimal`, `conf/curl`. Installed when `$(confdir)` does not exist (`Makefile.am` `samples-conf`). |
| Verto | HTML5 / WebRTC endpoint (`mod_verto`, name `verto.rtc`) | JSON-RPC 2.0 over WS/WSS. Methods in `src/mod/endpoints/mod_verto/mod_verto.c`. Needs libks. |

## Related Documentation

- [Project Overview](00-overview.md)
- [Architecture](02-architecture.md)
- [Tech Stack](03-tech-stack.md)
- [Data and API](05-data-and-api.md)
- [FAQ](appendix-01-faq.md)
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
