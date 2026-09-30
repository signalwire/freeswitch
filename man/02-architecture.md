# 02. Architecture

<!-- maintained-by: human+ai -->

C4 view of this FreeSWITCH tree. Product scope is in [Overview](00-overview.md); directory ownership in [Repository Map](04-repo-map.md). Loadable modules live **in-process** as DSOs — they are components, not separate deployable containers.

Two different “modules.conf” files matter ([Users Manual Chapter 5](https://developer.signalwire.com/freeswitch/configuration/module-loading/)):

| File | When it applies | Role |
|------|-----------------|------|
| `build/modules.conf.in` → `modules.conf` | Build time | Which `src/mod/...` trees are compiled |
| `conf/vanilla/autoload_configs/modules.conf.xml` | Runtime | Which compiled modules `switch_loadable_module_init()` actually loads |
| `conf/vanilla/autoload_configs/pre_load_modules.conf.xml` | Runtime, **before** the main list | Vanilla loads `mod_pgsql` here so DB backends exist before other modules init |
| `conf/vanilla/autoload_configs/post_load_modules.conf.xml` | Runtime, **after** the main list | Empty in vanilla |

A module must be compiled before it can appear in `modules.conf.xml`. Enabling a `<load>` for a missing `.so` logs an error and continues; compiling a module but leaving it commented out in XML is harmless.

## Three configuration domains

Operator config is three domains ([Users Manual Chapter 1](https://developer.signalwire.com/freeswitch/foundations/introduction)), not one XML blob:

| Domain | Question | Vanilla tree |
|--------|----------|--------------|
| Directory | Who is allowed to connect? | `conf/vanilla/directory/` |
| Dialplan | Where does a call go? | `conf/vanilla/dialplan/` |
| Configuration | How do core and modules behave? | `conf/vanilla/autoload_configs/` plus `sip_profiles/` |

Runtime objects the manual names (mapped to this tree):

| Term | Meaning | In this tree |
|------|---------|--------------|
| Endpoint | Protocol interface that originates or receives calls | `mod_sofia`, `mod_verto`, … under `src/mod/endpoints/` |
| Channel | One leg between FreeSWITCH and one endpoint | `switch_channel_t` |
| Session | Runtime container for a channel plus application state | `switch_core_session_t` |
| Call | One or more associated channels | Core SQL `calls` (`caller_uuid` / `callee_uuid`) |
| Bridge | Association that joins two channels’ media | `switch_ivr_bridge` / `bridge` app |

Channel variables use `${name}` at **call time**. Preprocessor variables use `$${name}` and expand **once** while assembling XML (`vars.xml`). Do not mix them ([Chapter 3](https://developer.signalwire.com/freeswitch/configuration/xml)).

## Context

FreeSWITCH is one softswitch process that terminates signaling and media, executes a dialplan, and emits events. Callers are SIP phones, trunks, and WebRTC browsers. Operators edit XML and use `fs_cli`. Application code either embeds (Lua/Python/V8 modules) or sits **outside** the process on the Event Socket (ESL). SignalWire is an optional cloud peer via `mod_signalwire`.

```{mermaid}
C4Context
    title C4 Context — FreeSWITCH
    Person(operator, "Operator / SRE", "XML, CLI, logs")
    Person(appdev, "App developer", "ESL or embedded scripts")
    Person(user, "End user", "Phones, browsers, trunks")
    System(fs, "FreeSWITCH", "Sessions, media, dialplan, modules")
    System_Ext(sip, "SIP peers", "UA / SBC / ITSP")
    System_Ext(webrtc, "Browsers", "Verto / WSS")
    System_Ext(sw, "SignalWire", "Optional pairing")
    System_Ext(esl, "ESL client", "fs_cli or custom")
    Rel(user, sip, "Calls")
    Rel(user, webrtc, "Calls")
    Rel(sip, fs, "SIP + RTP/SRTP")
    Rel(webrtc, fs, "Verto + RTP")
    Rel(esl, fs, "TCP 8021")
    Rel(operator, fs, "Config / CLI")
    Rel(fs, sw, "mod_signalwire")
    Rel(appdev, esl, "Writes")
```

UML nearest match: system-context diagram (no standard UML type).

## Container View

Deployable runtime pieces. Modules are **not** listed here; they load into the `freeswitch` process.

| Container | Responsibility | Technology | Depends on |
|-----------|----------------|------------|------------|
| `freeswitch` process | Sessions, state machine, media, module host, XML registry, event bus | C, APR, POSIX threads / Windows service | XML on disk, core SQLite (or ODBC/pgsql), RTP ports, loaded DSOs |
| XML config tree | Dialplan, directory, SIP profiles, module autoload | Preprocessed XML (`X-PRE-PROCESS` / `#include`) | Flattened to `freeswitch.xml.fsxml` at start (`conf/vanilla/freeswitch.xml`) |
| Core database | Internal scoreboard / recovery / some module data | SQLite by default (`sqlite3_initialize` in `switch_core_init`); optional ODBC/pgsql | `db/` under prefix |
| ESL client (`fs_cli` or custom) | Out-of-process API + event subscribe | TCP to `mod_event_socket` | Loopback 8021 by default |
| SIP / WebRTC peers | Signaling and media | Sofia-SIP, Verto | UDP/TCP 5060/5080, WSS, RTP ranges |

```{mermaid}
flowchart LR
    subgraph host["Host"]
      xml["XML confdir"]
      db["Core SQLite / ODBC"]
      fs["freeswitch process"]
      dso["Loaded .so modules"]
      fs --- dso
      xml --> fs
      fs --> db
    end
    sip["SIP UA / trunk"] -->|SIP+RTP| fs
    verto["Browser"] -->|Verto+RTP| fs
    cli["fs_cli / ESL app"] -->|TCP 8021| fs
```

UML nearest match: deployment / component diagram.

Default install prefix is `/usr/local/freeswitch` (`configure.ac`). One process per host is the unit of deployment; extra hosts are operator-owned HA, not an in-tree orchestrator. `switchname` in `conf/vanilla/autoload_configs/switch.conf.xml` only overrides hostname for DB/CURL identity in clustered configs.

## Component View

### `freeswitch` process

```{mermaid}
flowchart TB
    main["switch.c main"] --> init["switch_core_init_and_modload"]
    init --> xml["XML registry switch_xml.c"]
    init --> mods["Module loader switch_loadable_module.c"]
    init --> ev["Event engine switch_event.c"]
    mods --> ep["Endpoints: Sofia, Verto, loopback, RTC"]
    mods --> dp["Dialplan: mod_dialplan_xml"]
    mods --> app["Applications: dptools, conference, voicemail, ..."]
    mods --> esl["mod_event_socket"]
    mods --> log["Loggers: console, logfile"]
    ep --> sess["Session + channel + state machine"]
    dp --> sess
    app --> sess
    sess --> media["RTP / core media / codecs"]
    sess --> ev
    esl --> ev
```

#### Core runtime

- **Responsibilities**: APR/memory pools, directories, SQLite, SQL flags, session thread pool (`SCF_SESSION_THREAD_POOL`), signal handlers, `SWITCH_EVENT_STARTUP`.
- **Key entry points**: `src/switch.c` (`main`), `switch_core_init()` / `switch_core_init_and_modload()` in `src/switch_core.c`.
- **Important dependencies**: bundled APR (`libs/apr`), SQLite, `libs/srtp`, `libs/libvpx`.
- **Failure modes**: init aborts if APR or SQLite cannot start; `SCF_NO_NEW_SESSIONS` is set until the core is ready.

#### XML configuration

- **Responsibilities**: parse `conf_dir/freeswitch.xml`, expand preprocessor includes (`X-PRE-PROCESS` / `#include` / `#set`), serve sections (`configuration`, `dialplan`, `directory`, `languages`, `chatplan`). `mod_xml_curl` / `mod_xml_ldap` can replace file-backed sections at runtime by installing `switch_xml_set_open_root_function()`.
- **Key entry points**: `src/switch_xml.c` (`switch_xml_open_root`, `__switch_xml_open_root`), root document `conf/vanilla/freeswitch.xml`.
- **Compiled artifact**: `log_dir/freeswitch.xml.fsxml` (comment in `conf/vanilla/freeswitch.xml`: `${prefix}/log/freeswitch.xml.fsxml`). Memory-mapped while running — do not edit it.
- **Failure modes**: broken include path or XML error leaves the switch unable to open root (`"Cannot Open log directory or XML Root!"`).

#### Session, channel, state machine

- **Responsibilities**: each call leg is a `switch_core_session_t` (thread, codecs, queues, UUID) wrapping a `switch_channel_t` (state, variables, cause). The core runs `switch_state_handler_table` hooks; endpoints override them (Sofia’s `sofia_on_init` / `sofia_on_routing`).
- **Key entry points**: `src/switch_core_session.c`, `src/switch_channel.c`, `src/switch_core_state_machine.c`, types in `src/include/switch_types.h` (`switch_channel_state_t`).
- **Failure modes**: hung sessions if an endpoint returns without advancing state; media/proxy flags (`CF_PROXY_MODE`, `CF_PROXY_MEDIA`) change SDP/RTP ownership.

Default inbound path after create: `CS_NEW` → `CS_INIT` → `CS_ROUTING` (dialplan hunt) → `CS_EXECUTE` (apps) → `CS_EXCHANGE_MEDIA` or hangup path `CS_HANGUP` → `CS_REPORTING` → `CS_DESTROY`.

```{mermaid}
stateDiagram-v2
    [*] --> CS_NEW
    CS_NEW --> CS_INIT
    CS_INIT --> CS_ROUTING: standard on_init
    CS_ROUTING --> CS_EXECUTE
    CS_EXECUTE --> CS_EXCHANGE_MEDIA
    CS_EXECUTE --> CS_PARK
    CS_EXCHANGE_MEDIA --> CS_HANGUP
    CS_PARK --> CS_HANGUP
    CS_HANGUP --> CS_REPORTING
    CS_REPORTING --> CS_DESTROY
    CS_DESTROY --> [*]
```

#### Module loader

- **Responsibilities**: dlopen modules from `mod_dir`, register interface tables (endpoint, dialplan, application, codec, API, file, say, ASR, …) on `switch_loadable_module_interface`.
- **Key entry points**: `src/switch_loadable_module.c`, `src/include/switch_loadable_module.h`, `src/include/switch_module_interfaces.h`. Modules export `SWITCH_MODULE_DEFINITION` / `SWITCH_MODULE_LOAD_FUNCTION`.
- **Failure modes**: missing `.so` or load error is logged; `switch_core_init_and_modload` fails the whole startup if `switch_loadable_module_init(SWITCH_TRUE)` fails.

#### Media path

- **Responsibilities**: RTP/SRTP, jitter buffer, codecs, video, media bugs (tap/spy).
- **Key entry points**: `src/switch_rtp.c`, `src/switch_core_media.c`, `src/switch_jitterbuffer.c`, codec modules under `src/mod/codecs/`.
- **Failure modes**: NAT, wrong ptime (default 20 ms assumption in `switch.conf.xml`), codec mismatch forcing transcoding.

#### Signaling endpoints (in-process)

- **Sofia (`mod_sofia`)**: SIP. State handlers in `src/mod/endpoints/mod_sofia/mod_sofia.c`. Profiles in `conf/vanilla/sip_profiles/` (internal auth vs external `auth-calls=false`). Vanilla **internal** `context` is `public`; authenticated directory users override with `user_context` ([Chapter 7](https://developer.signalwire.com/freeswitch/users-and-endpoints/sip-profiles)).
- **Verto (`mod_verto`)**: HTML5/WebRTC, endpoint name `verto.rtc`.
- **Loopback / RTC**: in-process and WebRTC helper endpoints loaded by vanilla `modules.conf.xml`.

Media mode on a Sofia profile ([Chapter 17](https://developer.signalwire.com/freeswitch/media-and-codecs/handling)): vanilla internal sets `inbound-late-negotiation=true` (defer A-leg codec until after dialplan). `inbound-bypass-media` sets `CF_PROXY_MODE` (RTP endpoint-to-endpoint); `inbound-proxy-media` sets `CF_PROXY_MEDIA` (RTP through the server without payload inspection).

#### Dialplan and applications

- **Dialplan**: `mod_dialplan_xml` matches XML extensions (`src/mod/dialplans/mod_dialplan_xml/mod_dialplan_xml.c`). Asterisk-style `mod_dialplan_asterisk` is optional.
- **Applications**: `mod_dptools` (bridge, answer, playback, …), `mod_conference`, `mod_voicemail`, `mod_commands` (CLI/API). Originate lives in core `switch_ivr_originate()` (`src/switch_ivr_originate.c`).

#### Event engine and ESL

- **Bus**: APR FIFO + backend thread (`src/include/switch_event.h`). Bind callbacks or consume from `mod_event_socket`.
- **ESL**: `mod_event_socket` listens (default `127.0.0.1:8021`, password `ClueCon`). `fs_cli` is built from `libs/esl/fs_cli.c`. Auth flag `LFLAG_AUTHED` in `mod_event_socket.c`. Slow consumers must queue locally — the core warns against blocking the delivery thread.

#### Logging

- `mod_console`, `mod_logfile` (vanilla loads both). Levels via `SWITCH_CHANNEL_LOG` macros in `switch_types.h`.

## Code-Level View

Only types on the hot path. Full headers are the API; this is the map.

| Area | Important files or symbols | Why it matters |
|------|-----------------------------|----------------|
| Process boot | `src/switch.c` `main`; `switch_core_init`, `switch_core_init_and_modload` | Single-runlevel init; modules load only in the second function |
| Session | `struct switch_core_session` in `src/include/private/switch_core_pvt.h`; `switch_core_session_request_uuid` | UUID, endpoint pointer, codec pair, event/message queues, media bugs |
| Channel | `switch_channel_t`, `switch_channel_state_t` (`CS_*`), call state `CCS_*` | Dialplan and apps key off channel state/variables |
| Endpoint contract | `switch_endpoint_interface_t`, `switch_state_handler_table` | Sofia fills `on_init` / `on_routing` / … |
| Module contract | `switch_loadable_module_interface` | Named tables the core looks up (endpoint, app, codec, API) |
| Events | `switch_event_t`, `SWITCH_EVENT_CHANNEL_*`, `SWITCH_EVENT_STARTUP` | ESL, CDR, and scripts subscribe here |
| XML | `switch_xml_open_root`, preprocessor in `conf/vanilla/freeswitch.xml` | Config is data, not code |
| Originate / bridge | `switch_ivr_originate`, `src/switch_ivr_bridge.c` | Outbound legs and B2BUA |

```{mermaid}
classDiagram
    class switch_core_session_t {
        +pool
        +thread
        +endpoint_interface
        +channel
        +uuid_str
        +read_codec / write_codec
        +event_queue
    }
    class switch_channel_t {
        +state CS_*
        +variables
        +cause
    }
    class switch_endpoint_interface_t {
        +io_routines
        +state_handler
    }
    class switch_state_handler_table {
        +on_init
        +on_routing
        +on_execute
        +on_hangup
        +on_destroy
    }
    class switch_loadable_module_interface {
        +endpoint_interface
        +dialplan_interface
        +application_interface
        +codec_interface
        +api_interface
    }
    switch_core_session_t --> switch_channel_t
    switch_core_session_t --> switch_endpoint_interface_t
    switch_endpoint_interface_t --> switch_state_handler_table
    switch_loadable_module_interface --> switch_endpoint_interface_t
```

UML nearest match: class diagram + state machine (channel states above).

## Key Runtime Flows

### Inbound SIP call (vanilla)

```{mermaid}
sequenceDiagram
    participant UA as SIP UA
    participant Sofia as mod_sofia
    participant Core as Core session/SM
    participant DP as mod_dialplan_xml
    participant App as applications
    participant RTP as switch_rtp
    participant ESL as mod_event_socket

    UA->>Sofia: INVITE
    Sofia->>Core: switch_core_session_request + thread
    Core->>ESL: SWITCH_EVENT_CHANNEL_CREATE
    Core->>Sofia: on_init (sofia_on_init)
    Core->>Sofia: on_routing
    Sofia-->>UA: 100 Trying (if auto-invite-100)
    Core->>DP: CS_ROUTING hunt XML
    DP-->>Core: extension / apps
    Core->>App: CS_EXECUTE (e.g. bridge)
    App->>RTP: media
    UA->>RTP: RTP
    Core->>ESL: CHANNEL_EXECUTE / BRIDGE / HANGUP
    Core->>Core: CS_HANGUP → CS_REPORTING → CS_DESTROY
```

- **Success path**: INVITE → session thread → INIT/ROUTING → XML extension → apps (bridge/conference/voicemail) → RTP → hangup/CDR events.
- **Validation boundaries**: Sofia profile ACL (`apply-inbound-acl` on internal profile uses `domains` from `acl.conf.xml`); `auth-calls` on internal vs `false` on external; directory digest auth for users 1000–1019 in vanilla. Authenticated INVITE uses directory `user_context` (`default`); the profile’s own `context=public` is for unauthenticated inbound only ([Chapter 1](https://developer.signalwire.com/freeswitch/foundations/introduction) anatomy of a call).
- **Consistency**: one session thread owns the channel state machine. SQL scoreboard is internal (disable with `-nosql`). Channel recovery flags `CF_RECOVERING` skip routing and jump to `CS_EXECUTE`.
- **Retry / compensation**: SIP retransmits are Sofia/Sofia-SIP’s job. Dialplan `bridge` failover is application-level (pipe-separated gateways), not a core saga. ESL clients must not block the event thread.

### Outbound originate

`switch_ivr_originate()` allocates an outbound session through the named endpoint (`sofia/gateway/...`, `loopback/...`), runs the same state table with `TFLAG_OUTBOUND` so `sofia_on_init` sends INVITE via `sofia_glue_do_invite()`.

### ESL control

Client connects to 8021, authenticates, then `api` / `bgapi` / `event` / `sendmsg` (uuid). Health check used by Docker: `fs_cli -x status` must match `^UP`.

## Cross-Cutting Concerns

- **Authentication and authorization**: SIP digest + ACL lists (`conf/vanilla/autoload_configs/acl.conf.xml`, Sofia `apply-inbound-acl`). ESL password in `event_socket.conf.xml` (default `ClueCon`, loopback only). Directory users are **not** OS users.
- **Error handling**: `switch_status_t` and hangup `switch_call_cause_t` on the channel. Endpoints return `SWITCH_STATUS_FALSE` from a state handler to skip the core’s standard handler (`sofia_on_init` comment).
- **Logging and tracing**: `switch_log_printf` with session UUID via `SWITCH_CHANNEL_SESSION_LOG`. Sofia `siptrace` is a profile/CLI switch, not distributed tracing. No OpenTelemetry in this tree.
- **Caching**: compiled XML root is memory-mapped (`freeswitch.xml.fsxml`); `reloadxml` re-parses. Directory/ACL can be rebuilt from XML. RTP jitter buffer is per-session, not a shared cache.
- **Configuration and secrets**: XML under `confdir`; `vars.xml` preprocessor `$$` vars (including `default_password`). Not a secret manager. Change demo passwords before any public bind (`conf/vanilla/README_IMPORTANT.txt`).

## Deployment and Scaling Notes

- **Environments**: same binary; profile choice (`vanilla`, `sbc`, `minimal`, `curl`) is the environment. CI builds on Debian containers, macOS, Windows.
- **Scaling model**: **vertical** (more cores / less transcoding) and **more processes/hosts** behind an SBC or Kamailio. Not a horizontally sharded in-process cluster. RTP needs host networking in Docker (`docker/README.md`).
- **Recovery**: `-nonat` to skip UPnP pinholes; session recovery flags in the state machine; core SQL can persist some state. Site HA, backups, and RTO are **[NEEDS INPUT: operator-owned]** — not specified in this repo.

## Related Documentation

- [Overview](00-overview.md)
- [Tech Stack](03-tech-stack.md)
- [Repository Map](04-repo-map.md)
- [Data and API](05-data-and-api.md)
- [Workflows](06-workflows.md)
- [ADRs](adr/index.md)
- [Users Manual Ch 1](https://developer.signalwire.com/freeswitch/foundations/introduction), [Ch 3 XML](https://developer.signalwire.com/freeswitch/configuration/xml), [Ch 5 modules](https://developer.signalwire.com/freeswitch/configuration/module-loading/)

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
