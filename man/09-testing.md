# 09. Testing Strategy

<!-- maintained-by: human+ai -->

How this tree is validated: C unit binaries under `tests/unit/`, per-module Autotools `TESTS` collected by `make print_tests`, ESL `make check`, clang `scan-build`, then a short live-process smoke after `make install`. GitHub Actions is the source of truth for the automated path (`.github/workflows/ci.yml`). There is no published coverage percentage in this repo.

New core tests must use `src/include/test/switch_test.h` (`tests/unit/README`). Do not use `scripts/debian_min_build.sh` as a test or build environment (Jessie / FreeSWITCH 1.6 pins; see [Quick Start](01-quick-start.md)).

## Test Layers

| Layer | Primary checks | Ownership / notes |
|-------|----------------|-------------------|
| Core C unit binaries | `cd tests/unit && ./run-tests.sh` after configure/build/install. Programs in `tests/unit/Makefile.am` `noinst_PROGRAMS`, listed as `TESTS`. | libfreeswitch / core. Framework: FCT via `src/include/test/switch_test.h` and `src/include/test/switch_fct.h`. Each binary is a separate process. |
| Per-module tests | Same runner: top-level `make print_tests` also walks `src/mod` (`OUR_TEST_MODULES` → `<mod>-print_tests` in `configure.ac`). | Module `Makefile.am` that sets `TESTS`. Only enabled modules in `modules.conf` are printed. |
| ESL library | `make -C libs/esl check` | `libs/esl/tests/test_recv_event.c`. CI runs this only on **group 1** (`.github/workflows/unit-test.yml`). |
| Static analysis | `./ci.sh -t scan-build -a configure\|build\|validate -c freeswitch` | `.github/workflows/scan-build.yml` + `scan-build-14`. Fails if the log does not contain `scan-build: No bugs found`. |
| Platform compile | `macos.yml` Homebrew build; `windows.yml` MSBuild `Freeswitch.2017.sln` | Compile/install (and Windows MSI), **not** `run-tests.sh`. A subset of unit sources have `.2017.vcxproj` entries in the solution. |
| Live smoke | `fs_cli -x status` after a prefix install | Manual / Docker healthcheck. Complements unit tests; does not replace them. |
| PKB docs | `cd man && poetry install && make html-en && make pkb-check` | This Sphinx tree (`man/`), not `docs/man/` Unix pages. Not part of `.github/workflows/ci.yml`. |

`tests/unit/switch_eavesdrop.c` is built as `bin_PROGRAMS` example only (`examples = switch_eavesdrop`); `Makefile.am` comments that `make check` will not run it.

## Core Regression Areas

These are the `tests/unit/` programs in `noinst_PROGRAMS` (plus `switch_rtp_pcap` when `pcap-config` exists — `HAVE_PCAP` in `configure.ac`). Names match the C files.

| Area | Program | What it exercises |
|------|---------|-------------------|
| Core utilities | `switch_core` | IP/CIDR, regex, hashes, UUID v4/v7, spawn vs system, XML attrs, event header leak |
| Sessions | `switch_core_session` | External session ID locate/hangup (`FST_SESSION_BEGIN(session_external_id)`) |
| Originate / dialplan IVR | `switch_ivr_originate` | Empty dial string, group-confirm legs/timeouts, JSON dial handles, video, enterprise originate |
| Play / record | `switch_ivr_play_say`, `switch_ivr_async` | Collect-input success/failure/partial; async record pause and channel vars |
| RTP / RTCP | `switch_rtp` | RTP session, RTCP audio event, client cert verify |
| RTP pcap (optional) | `switch_rtp_pcap` | Stall with muxed RTCP, oversized NACK length, media timeout. Needs libpcap. Fixtures in `tests/unit/pcap/` |
| SIP identity | `switch_sip` | STIR/SHAKEN compact vs full Identity (`conf_sip/`) |
| Sofia load | `test_sofia` | Loads `mod_sofia` (`sofia_leaks` body is empty — ASAN leak check on process exit) |
| Event Socket | `test_mod_event_socket` | ESL Content-Length validation and `sendevent` body round-trip (`conf_event_socket/`) |
| Verto HTTP | `test_mod_verto` | POST at cap / overflow → 413 (`conf_verto/`) |
| XML parser | `switch_xml` | CDATA, UTF-8, DTD on/off, entity-expansion limits (XXE-style) |
| Events / hash / console | `switch_event`, `switch_hash`, `switch_console` | Minimal core (`FST_MINCORE_BEGIN`); includes micro-benchmarks |
| STUN | `switch_stun` | IPv4/IPv6 mapped addresses, MESSAGE-INTEGRITY, truncated attributes |
| Hold | `switch_hold` | Hold/unhold restriction (`conf_test/`) |
| Media / codecs | `switch_core_media`, `switch_core_codec`, `switch_vad`, `switch_packetizer`, `switch_vpx`, `switch_core_asr` | Crypto key-salt bounds, codec copy, VAD, H.264 packetizer, VP8, ASR resample |
| Files / DB / log / TTS | `switch_core_file`, `switch_core_db`, `switch_log`, `test_tts_format` | File close/write channels, cache DB race, JSON log metadata, `tts://` channel UUID |
| Utils | `switch_utils` | Base64 bounds and HTTP header parse |
| Video images | `switch_core_video` | Patch/alpha using `tests/unit/images/` |

**Module `TESTS` (also on the `print_tests` list when the module is enabled):**

| Module | Tests |
|--------|-------|
| `src/mod/endpoints/mod_sofia` | `test/test_sofia_funcs.sh`, `test/test_nuafail`, `test/test_603plus`. SIPP (`test/test_run_sipp.sh`) is **commented out** in `Makefile.am` |
| `src/mod/applications/mod_commands` | `test/test_mod_commands`, `test/test_interface_allowlist` |
| `src/mod/applications/mod_conference` | `test/test_image`, `test/test_member` |
| `src/mod/applications/mod_http_cache` | `test/test_aws` |
| `src/mod/applications/mod_av` | `test/test_mod_av`, `test/test_avformat` |
| `src/mod/applications/mod_test` | `test/test_asr`, `test/test_tts` |
| `src/mod/languages/mod_lua` | `test/test_mod_lua` |
| `src/mod/formats/mod_sndfile` | `test/test_sndfile`, `test/test_sndfile_conf` |
| `src/mod/formats/mod_opusfile` | `test/test_opusfile` |
| `src/mod/codecs/mod_amr` | `test/test_amr` |
| `src/mod/codecs/mod_amrwb` | `test/test_amrwb` |
| `src/mod/codecs/mod_openh264` | `test/test_mod_openh264` |

`src/mod/timers/mod_posix_timer/test/` has C sources but **no** `TESTS` in that module’s `Makefile.am`, so `print_tests` will not emit them.

CI unit-test configure (`ci.sh -t unit-test`) extra-enables `mod_http_cache`, `mod_opusfile`, `mod_lua`, and appends `codecs/mod_openh264` so those module tests can appear in the list.

## Test Data and Environments

- **Fixtures**: per-suite XML under `tests/unit/conf/` (shared core), `conf_sip/`, `conf_sofia/`, `conf_rtp/`, `conf_stun/`, `conf_verto/`, `conf_event_socket/`, `conf_eavesdrop/`, `conf_test/`, `conf_tts_format/`, `conf_playsay/`, `conf_async/`. RTP pcaps: `tests/unit/pcap/milliwatt*.pcap`. Images: `tests/unit/images/{banner,signalwire}.png`. Module tests often ship their own `test/conf` (for example `src/mod/endpoints/mod_sofia/test/`).
- **State isolation**: `FST_CORE_BEGIN` / `FST_CORE_DB_BEGIN` / `FST_MINCORE_BEGIN` in `switch_test.h` call `fst_init_core_and_modload()`, point `conf_dir` at the suite directory, and put `log_dir` / `db_dir` under a **PID** subdirectory. `FST_CORE_END` calls `switch_core_destroy()`. Tests run **one binary at a time** inside a chunk (`run-tests.sh` sequential `make -f run-tests.mk`). CI uses `--privileged` and writes cores to `/cores/core.%s.%E.%e.%p.%t`.
- **External dependencies**: Sofia-SIP is cloned from `freeswitch/sofia-sip` (not vendored). Unit-test image: `signalwire/freeswitch-public-ci-base:bookworm-amd64`. Extra apt in `unit-test.yml`: `libpcre2-dev`, `libsphinxbase-dev`, `libpocketsphinx-dev`. ASAN: `--enable-address-sanitizer --enable-fake-dlclose` plus `ASAN_OPTIONS=log_path=stdout:disable_coredump=0:unmap_shadow_on_exit=1:fast_unwind_on_malloc=0` (workflow env and `ci.sh`).
- **Chunking**: `./run-tests.sh [chunks] [chunk_number]` round-robins the `print_tests` list. Default CI is **2 groups** (`ci.yml` `TOTAL_GROUPS: 2`). Optional DinD (`workflow_dispatch` input `dind`) uses `tests/unit/run-tests-docker.sh` with `MAX_CONTAINERS: 8` (`.github/workflows/unit-test-dind.yml`) and `.github/docker/debian/bookworm/amd64/CI/Dockerfile`.

Failed binaries keep `log_run-tests_*.html` (via `ansi2html` in `test.sh`); `collect-test-logs.sh --dir logs --print` builds `artifacts.html` and dumps text when `html2text` is installed.

## Validation Commands

CI-equivalent (Sofia-SIP already built/installed; same actions as `unit-test.yml`):

```bash
./ci.sh -t unit-test -a configure -c freeswitch
./ci.sh -t unit-test -a build -c freeswitch
./ci.sh -t unit-test -a install -c freeswitch

cd tests/unit
./run-tests.sh                         # all tests from print_tests
./run-tests.sh 2 1                     # group 1 of 2 (GitHub Actions pattern)
./run-tests.sh 2 1 --dry-run           # list only
./run-tests.sh 2 1 --output-dir logs   # move HTML/backtraces into logs/

make -C ../../libs/esl check           # ESL; CI does this on group 1 only
./collect-test-logs.sh --dir logs --print
```

Sofia-SIP first (from repo root, path to a sofia-sip checkout):

```bash
./ci.sh -t unit-test -a configure -c sofia-sip -p /path/to/sofia-sip
./ci.sh -t unit-test -a build     -c sofia-sip -p /path/to/sofia-sip
./ci.sh -t unit-test -a install   -c sofia-sip -p /path/to/sofia-sip
```

After a normal `./configure && make && make install` (without `ci.sh` ASAN flags), the same `tests/unit/run-tests.sh` still applies; `make print_tests` from the **build root** is what the runner calls (`tests/unit/run-tests.sh`). Top-level `make check` recurses `src/mod` (`check: $(OUR_CHECK_MODULES)`) and `tests/unit` (`SUBDIRS`). That runs enabled-module `TESTS` plus core unit binaries. It does **not** run `libs/esl` — that tree is not in top-level `SUBDIRS`; CI calls `make -C libs/esl check` separately.

Static analysis:

```bash
./ci.sh -t scan-build -a configure -c sofia-sip -p /path/to/sofia-sip
./ci.sh -t scan-build -a build     -c sofia-sip -p /path/to/sofia-sip
./ci.sh -t scan-build -a install   -c sofia-sip -p /path/to/sofia-sip
./ci.sh -t scan-build -a configure -c freeswitch
./ci.sh -t scan-build -a build     -c freeswitch   # needs scan-build-14
./ci.sh -t scan-build -a validate  -c freeswitch
```

Optional local Docker runner (needs a Sofia-SIP tree and Docker):

```bash
./tests/unit/run-tests-docker.sh \
  --sofia-sip-path /path/to/sofia-sip \
  --freeswitch-path "$(pwd)" \
  --base-image signalwire/freeswitch-public-ci-base:bookworm-amd64 \
  --max-containers 2 \
  --cpus 1 \
  --output-dir tests/unit/logs
```

Windows (build only in CI): unit vcxproj in `Freeswitch.2017.sln` — `test_switch_core`, `test_switch_core_db`, `test_switch_core_codec`, `test_switch_ivr_originate`, `test_switch_xml`, `test_tts_format`.

## Manual Smoke Checks

Automation does not start a long-lived vanilla PBX and place a SIP call. After [Quick Start](01-quick-start.md) install:

1. `"$prefix/bin/freeswitch" -ncwait -nonat` then `"$prefix/bin/fs_cli" -x status` — line starts with `UP` (same check as `docker/base_image/healthcheck.sh`).
2. `fs_cli -x sofia status` — internal/external profiles present when `mod_sofia` is loaded.
3. `fs_cli -x "module_exists mod_event_socket"` and `fs_cli -x version` — ESL on `127.0.0.1:8021` password `ClueCon` unless you changed it.
4. Directory users `1000`–`1019` exist in vanilla config — **demo passwords**; do not expose on a public IP. A two-phone register/call is the usual media/SIP smoke; there is no in-tree script that places that call.
5. Docker: `docker/README.md` expects **host networking**; RTP/SIP published-port setups are a common false failure.

## Documentation Verification

PKB lives in `man/` at the repo root (not `docs/man/`).

```bash
cd man
poetry install
make html-en          # _build/site/en/
make pkb-check        # scripts/check_pkb_staleness.py
make pkb-check-i18n   # zh_CN catalog drift, if locale files exist
```

Bilingual HTML: `make html-all` (see [Build](08-build.md) and [Documentation Process](12-document.md)).

## Regression Additions

When a bug is fixed, add the smallest check that would have failed before the fix:

1. Prefer a new `FST_TEST_BEGIN` / `FST_SESSION_BEGIN` in an existing `tests/unit/*.c` that already loads the right `conf_*` tree.
2. New file: add the `.c` to `tests/unit/Makefile.am` `noinst_PROGRAMS` (that list **is** `TESTS`). Use `src/include/test/switch_test.h` only.
3. Module-specific: add `noinst_PROGRAMS` + `TESTS` in that module’s `Makefile.am` so `print_tests` emits `src/mod/.../test/...`.
4. ESL protocol bugs: extend `libs/esl/tests/` (Autotools `TESTS`) rather than only a live `fs_cli` check.
5. Record here: original failure, expected behavior, and the command (`./run-tests.sh` vs `make -C libs/esl check` vs a smoke step).

Existing tests that already look like regression/security locks: XML entity expansion (`switch_xml`), STUN integrity/truncation (`switch_stun`), ESL Content-Length (`test_mod_event_socket` and `libs/esl/tests/test_recv_event.c`), Verto POST 413 (`test_mod_verto`), RTP NACK length (`switch_rtp_pcap`), crypto key-salt bounds (`switch_core_media`).

[NEEDS INPUT: named production incidents or ticket IDs to attach to those tests — none are referenced in the C sources.]

## Related Documentation

- [Quick Start](01-quick-start.md) — local install and first `run-tests.sh`
- [Build, Release, and Publish](08-build.md) — `ci.yml` matrix, `ci.sh`, packaging
- [Workflows](06-workflows.md)
- [Runbook](10-runbook.md)
- [Observability](11-observability.md)
- [Documentation Process](12-document.md)

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
