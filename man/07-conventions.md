# 07. Conventions

<!-- maintained-by: human+ai -->

Verified rules from this tree: `docs/SubmittingPatches`, root `.clang-format`, module macros in `src/include/switch.h` / `switch_types.h`, and the two different `modules.conf` files. There is no `.editorconfig`, no `uncrustify.cfg`, and no repo-root pre-commit or clang-tidy gate.

## Style and Tooling

| Area | Rule | Source of truth |
|------|------|-----------------|
| C/C++ formatting | Tabs, 4-wide indent, 120-column limit, Linux brace style, right-aligned pointers. Match surrounding code; do not reformat unrelated lines. | Root `.clang-format` (`UseTab: Always`, `TabWidth`/`IndentWidth: 4`, `ColumnLimit: 120`, `BreakBeforeBraces: Linux`, `PointerAlignment: Right`). `docs/SubmittingPatches` (tabs, no trailing whitespace). |
| Formatter enforcement | `.clang-format` exists at the repo root but is **not** run by FreeSWITCH GitHub Actions. Do not assume a format-on-PR bot. Bundled `libs/srtp/` has its own `.clang-format` and Travis `format.sh`; that is not the core tree. | `.clang-format`; `.github/workflows/ci.yml`; `libs/srtp/format.sh` |
| Uncrustify / EditorConfig | Not present. Do not invent them. | No `uncrustify.cfg`, no `.editorconfig` |
| Whitespace footer | End `*.[ch]` with the Emacs/VIM local-variables block (`indent-tabs-mode:t`, `tab-width:4`, `c-basic-offset:4` / `vim:set … noet`). Strip trailing whitespace. | `docs/SubmittingPatches` “whitespace footer”; example `src/mod/applications/mod_skel/mod_skel.c` |
| Line endings | LF for `*.c` / `*.h` / `*.in` / `*.txt`. | `.gitattributes` |
| Static analysis | Clang `scan-build-14` on PRs via `ci.sh -t scan-build` (uses `build/modules.conf.most` plus explicit enable/disable edits). Address-sanitizer unit-test configure is a separate `ci.sh -t unit-test` path. | `ci.sh`; `.github/workflows/ci.yml` |
| Shell (`ci.sh` only) | Comment documents `shfmt -w -s -ci -sr -kp -fn ci.sh`. That is a helper-script convention, not a tree-wide shell linter. | `ci.sh` header |
| License header | New core/module C files carry the MPL 1.1 FreeSWITCH header used throughout `src/` and `src/include/`. | `LICENSE`; `src/include/switch.h` |
| Git identity | Run `./scripts/setup-git.sh` from the repo root (real name and email, not a username). It sets `pull.rebase` and `branch.master.rebase` to `true`. | `docs/SubmittingPatches`; `scripts/setup-git.sh` |
| Commits | Topic branch; one identifiable change per commit; present-tense imperative subject typically &lt; 50 characters; body hard-wrapped 68–72 characters; empty line after subject. Mention JIRA as `FS-XXXX` on its own line; add `#resolve` if the commit closes it. Rebase onto upstream; do not merge master into the topic branch. | `docs/SubmittingPatches` |
| Review path | GitHub pull requests are the inclusion path (`README.md`). CI on PR open/sync: unit tests + scan-build (`ci.yml`). Mailing-list patches are not the inclusion path (`docs/SubmittingPatches`). | `README.md`; `docs/SubmittingPatches`; `.github/workflows/ci.yml` |

Practical C style for this tree: **match surrounding code**, use **tabs**, keep the **Emacs/VIM footer**, and treat root `.clang-format` as the written style when you do format — not as a CI gate. There is no uncrustify config.

## Naming and Layout

- **Core files**: `src/switch_<area>.c` with matching `src/include/switch_<area>.h`. Process entry is `src/switch.c`. Public umbrella header is `src/include/switch.h` — module authors include that, not a private core `.c`.
- **Modules**: `src/mod/<category>/mod_<name>/` with primary source `mod_<name>.c` (or `.cpp`). Categories in this tree: `applications`, `endpoints`, `codecs`, `say`, `event_handlers`, `formats`, `languages`, `xml_int`, `loggers`, `dialplans`, `asr_tts`, `databases`, `timers`, `directories`, `sdk`.
- **Module API**: declare load/shutdown/runtime with `SWITCH_MODULE_LOAD_FUNCTION`, `SWITCH_MODULE_SHUTDOWN_FUNCTION`, `SWITCH_MODULE_RUNTIME_FUNCTION`, then `SWITCH_MODULE_DEFINITION(mod_name, load, shutdown, runtime)` (`src/include/switch_types.h`). Skeleton: `src/mod/applications/mod_skel/` and `build/standalone_module/mod_skel.c`.
- **Exported vs private**: symbols not meant to be exported are `static` (`docs/SubmittingPatches`). Public core APIs use `SWITCH_DECLARE(...)` (`src/include/switch_platform.h`). Register console APIs with `SWITCH_ADD_API` and dialplan apps with `SWITCH_ADD_APP` (`src/include/switch_loadable_module.h`).
- **Config files**: `<feature>.conf.xml` under `conf/<profile>/autoload_configs/` (or the module’s own `conf/` samples). Runtime module load list is XML `<load module="mod_…"/>`, not a `src/mod/…` path.
- **XML preprocessor**: `#include` / `#set` / `X-PRE-PROCESS`. You cannot comment out an `X-PRE-PROCESS` line — remove it (`conf/vanilla/vars.xml`). Flattened dump is `freeswitch.xml.fsxml`; do not edit it while the process is running.
- **PKB**: English source lives in `man/` (not `docs/man/`, which is Unix man pages). `man/pyproject.toml` is Sphinx/Poetry for this knowledge base — **not** product runtime dependencies.

### Two `modules.conf` files (do not mix)

| File | Role | Format |
|------|------|--------|
| `build/modules.conf.in` → copied to repo-root `modules.conf` by `bootstrap.sh` if missing | **Build**: which `src/mod/<category>/mod_*` trees are compiled | Lines like `endpoints/mod_sofia`; `#` comments disable |
| `conf/vanilla/autoload_configs/modules.conf.xml` (installed under `$prefix/conf/`) | **Runtime**: which already-built modules the process loads | `<load module="mod_sofia"/>` |

A module can be compiled and still not load, or listed in XML and missing from `mod/` because it was commented out at build time. `build/modules.conf.most` is the scan-build “almost everything” list, not the default.

## Error Handling

- **Classification**: C APIs return `switch_status_t` (`src/include/switch_types.h`). Common values: `SWITCH_STATUS_SUCCESS`, `SWITCH_STATUS_FALSE`, `SWITCH_STATUS_TIMEOUT`, `SWITCH_STATUS_RESTART`, `SWITCH_STATUS_INTR`, `SWITCH_STATUS_NOTIMPL`, `SWITCH_STATUS_MEMERR`, `SWITCH_STATUS_NOOP`, `SWITCH_STATUS_GENERR`, `SWITCH_STATUS_INUSE`, `SWITCH_STATUS_BREAK`, `SWITCH_STATUS_SOCKERR`, `SWITCH_STATUS_MORE_DATA`, `SWITCH_STATUS_NOTFOUND`, `SWITCH_STATUS_UNLOAD`, `SWITCH_STATUS_NOUNLOAD`, `SWITCH_STATUS_IGNORE`, `SWITCH_STATUS_TOO_SMALL`, `SWITCH_STATUS_FOUND`, `SWITCH_STATUS_CONTINUE`, `SWITCH_STATUS_TERM`, `SWITCH_STATUS_NOT_INITALIZED`, `SWITCH_STATUS_TOO_LATE`. Compare against these constants; do not invent ad-hoc integer codes for core/module APIs.
- **Load/runtime**: module load functions return `switch_status_t`. Runtime threads that should not be rescheduled return `SWITCH_STATUS_TERM` (see `mod_skel` comments). `SWITCH_STATUS_NOUNLOAD` means “never unload”.
- **Media reads**: `SWITCH_READ_ACCEPTABLE(status)` treats `SUCCESS`, `BREAK`, and `INUSE` as acceptable (`src/include/switch_utils.h`).
- **User-facing errors**: ESL replies are `+OK` / `-ERR` after `auth` (`mod_event_socket.c` `parse_command()`). Console/API strings go through `SWITCH_ADD_API`. Call teardown uses channel hangup causes, not a REST error body. SIP status is Sofia’s problem, not a second core errno.
- **Null/empty strings**: use `zstr()` (`switch_utils.h`). `switch_assert()` aborts on invariant failure (`switch_platform.h`) — not a substitute for returning `SWITCH_STATUS_*` on expected failures.
- **Retries or fallbacks**: there is no project-wide retry wrapper. `SWITCH_STATUS_RESTART` / `BREAK` / `TIMEOUT` are control-flow statuses for the caller. SIP retransmission lives in Sofia; do not add a generic “retry three times” layer in new module code unless that module already does so.

## Logging and Diagnostics

- **API**: `switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_<LEVEL>, "fmt\n", …)` (`src/include/switch_log.h`). The first argument is a **channel macro** that supplies file, function, and line (`SWITCH_CHANNEL_LOG`, `SWITCH_CHANNEL_LOG_CLEAN`, `SWITCH_CHANNEL_SESSION_LOG(session)`, `SWITCH_CHANNEL_UUID_LOG(uuid)` in `switch_types.h`). Optional JSON metadata: `switch_log_meta_printf`.
- **Levels** (`switch_log_level_t`): `DEBUG10`…`DEBUG1`, then `DEBUG` (7), `INFO` (6), `NOTICE` (5), `WARNING` (4), `ERROR` (3), `CRIT` (2), `ALERT` (1), `CONSOLE` (0), plus `DISABLE` / `INVALID` / `UNINIT`. Prefer `ERROR`/`CRIT` for failures, `WARNING` for recoverable misconfig, `DEBUG`/`DEBUG1`+ for packet/session chatter.
- **Required fields**: file, function, and line are automatic via the channel macros. Session-scoped logs should use `SWITCH_CHANNEL_SESSION_LOG(session)` so the UUID is attached. `conf/vanilla/autoload_configs/logfile.conf.xml` can prefix lines with the session UUID (`<param name="uuid" value="true"/>`). Log nodes also carry timestamp and a sequence (`switch_log_node_t`).
- **Where it goes**: console mappings in `console.conf.xml` (`$${console_loglevel}`); file logger `mod_logfile` (rotate-on-HUP, rollover). Operators map by file name, function name, or `all`.
- **Debug workflow**: raise console/file maps for the file under study (`mod_sofia.c`, `switch_core_state_machine.c`, …); `fs_cli` `/log debug`; `freeswitch -c -nonat` for a foreground console. Unit tests often start the core with `SCF_LOG_DISABLE` (`src/include/test/switch_test.h`).
- **Sensitive data policy**: never log ESL passwords, SIP digest credentials, voicemail PINs, SignalWire PATs, or XML-RPC `auth-pass` at info-or-higher in examples or new code as if they were production-safe. Demo values (`ClueCon`, `default_password=1234`, users 1000–1019) are **known-insecure samples**, not a logging exemption. Do not paste production auth into PKB examples.

## Configuration and Secrets

- **Config sources**: XML under `conf/<profile>/` (vanilla is the demo PBX). Preprocessor variables in `vars.xml` (`$${default_password}`, `$${console_loglevel}`, …). Module settings in `autoload_configs/*.xml`. Build-time module set: `modules.conf`. Install prefix and version: `configure.ac`.
- **Override order**: preprocessor `#set` / `X-PRE-PROCESS` expands before XML parse. Runtime `reloadxml` re-reads XML; it does not rebuild modules. Enabling a module requires **both** an uncommented build line **and** a runtime `<load>` (unless you `load mod_*` from the console).
- **Vanilla is demo, not production**: `conf/vanilla/README_IMPORTANT.txt`. Default directory users 1000–1019 share `$${default_password}` (`1234` in `vars.xml`); voicemail PINs match the extension. Dialplan logs CRIT and sleeps 10s while the password is still `1234` (`conf/vanilla/dialplan/default.xml`). Change passwords or run `scripts/perl/randomize-passwords.pl`. Start with `-nonat` unless UPnP/NAT-PMP pinholes are intentional.
- **ESL**: default password `ClueCon` (`conf/vanilla/autoload_configs/event_socket.conf.xml` listens on `::` port 8021; the module sample in `src/mod/event_handlers/mod_event_socket/conf/` binds `127.0.0.1`). `libs/esl/fs_cli.conf` uses the same password. **Change it** before any non-loopback exposure. Erlang cookie samples also use `ClueCon`.
- **XML-RPC demo auth** (not in vanilla autoload): `auth-user` `freeswitch` / `auth-pass` `works` (`src/mod/xml_int/mod_xml_rpc/conf/autoload_configs/xml_rpc.conf.xml`).
- **Packaging secrets**: FSGET and packaged Docker images need a SignalWire Personal Access Token (`scripts/packaging/README.md`). Pass the PAT on the command line or as a Docker `TOKEN` build-arg — **never commit it**. Source builds do not need that token (`docker/examples/Debian11/Dockerfile`).
- **No secrets in git**: do not add PATs, production SIP passwords, TLS private keys, or live ESL passwords to this repository. Demo XML that already contains `ClueCon` / `1234` stays demo; new config must not copy those values as if they were a baseline.
- **Version bump**: keep together (comment in `configure.ac`): `AC_INIT([freeswitch], […])`, `SWITCH_VERSION_MAJOR` / `MINOR` / `MICRO` (and optional `REVISION` / `REVISION_HUMAN` for a tagged release), and `build/next-release.txt`. See [Build](08-build.md).

## Testing and Review Expectations

- **Core unit tests**: Autotools programs under `tests/unit/` against `libfreeswitch`. New tests **must** use `src/include/test/switch_test.h` (`tests/unit/README`). Run `tests/unit/run-tests.sh` after configure/build/install; CI chunks with `./run-tests.sh 2 1` (`ci.yml` `TOTAL_GROUPS: 2`). ESL: `make -C libs/esl check` (CI group 1).
- **Module tests**: some modules ship `test/` trees (for example Sofia SIPp under `src/mod/endpoints/mod_sofia/test/`). Enable the module in **build** `modules.conf` before those tests can link.
- **CI on a GitHub PR**: `.github/workflows/ci.yml` (unit-test groups via `ci.sh`, optional DinD, `scan-build`). Platform packs: `build.yml`, `macos.yml`, `windows.yml`. Treat a red `ci.yml` as blocking.
- **New module checklist** (`docs/SubmittingPatches`): `Makefile.am`; entry in `configure.ac`; commented line in `build/modules.conf.in`; header comments describing the module; whitespace footer; `static` for private symbols; **do not** add files to `conf/vanilla`; commit body explains why it exists and what is unfinished.
- **Docs for changes**: behavior, flags, or XML knobs belong in the commit body. PKB pages under `man/` are English source for this knowledge base — update the affected numbered page when layout, build, or conventions change; do not treat `man/pyproject.toml` as a FreeSWITCH dependency bump.
- **Review focus**: media/SIP compatibility, module load vs build-list mistakes, secret leakage in samples, rebase-clean history, scan-build noise, and whether vanilla XML was treated as production.

## Anti-Patterns

- Mixing **build** `modules.conf` / `build/modules.conf.in` with **runtime** `conf/vanilla/autoload_configs/modules.conf.xml` (different files, different syntax, different effect).
- Treating `conf/vanilla` as a production baseline (`README_IMPORTANT.txt`). Shipping `default_password=1234`, users 1000–1019, ESL `ClueCon`, or XML-RPC `works` as if they were safe.
- Logging or documenting ESL `ClueCon` or SIP digest/voicemail secrets as production-ready examples.
- Committing a SignalWire PAT, Docker `TOKEN`, or any live credential. Putting secrets in git “just for CI”.
- Bumping only `AC_INIT` or only `build/next-release.txt` instead of all version fields together.
- Whitespace-only or drive-by reformat mixed into a functional commit; merge commits of master into a topic branch (`docs/SubmittingPatches`).
- Exporting internal module symbols; skipping `static`; omitting the Emacs/VIM footer on new `*.[ch]`.
- Adding sample XML to `conf/vanilla` as part of a new module PR.
- Commenting out `X-PRE-PROCESS` lines, or editing `freeswitch.xml.fsxml` while FreeSWITCH is running.
- Starting a public-facing box without `-nonat` and without ACLs on SIP/ESL ports.
- Treating `docs/man/` as this PKB, or `man/pyproject.toml` as product Python dependencies.
- Sending patches only to the mailing list and expecting them to land (`docs/SubmittingPatches`); skipping GitHub PR CI (`README.md`, `ci.yml`).

## Related Documentation

- [Quick Start](01-quick-start.md)
- [Repository Map](04-repo-map.md)
- [Data and API](05-data-and-api.md)
- [Build](08-build.md)
- [Testing](09-testing.md)
- [Observability](11-observability.md)
- [Documentation Process](12-document.md)
- `docs/SubmittingPatches`
- `conf/vanilla/README_IMPORTANT.txt`

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
