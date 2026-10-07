# 03. Tech Stack

<!-- maintained-by: human+ai -->

FreeSWITCH is a **C Autotools** softswitch. There is no `package.json` / `go.mod` / `Cargo.toml` for the product. `scripts/gen_tech_stack.py` only found the **PKB Sphinx** manifests under `man/` (see `_generated/03-tech-stack.generated.md`). Product versions below come from `configure.ac` `PKG_CHECK_MODULES` / `AC_CHECK_LIB` floors and from bundled `libs/` headers.

This tree is **1.11.3-dev** (`AC_INIT` in `configure.ac`).

## Purpose

A threaded C core (`libfreeswitch`) plus loadable DSOs implement SIP/WebRTC switching, RTP, dialplan, and ESL. System libraries (Sofia-SIP, SpanDSP, OpenSSL, libks) and a few vendored trees (`libs/apr`, `libs/srtp`, `libs/libvpx`, …) provide portability, crypto, and codecs.

## System View

```{mermaid}
flowchart TD
    ua["SIP / WebRTC UA"] --> ep["Endpoints: Sofia, Verto"]
    cli["fs_cli / ESL app"] --> esl["mod_event_socket TCP 8021"]
    ep --> core["libfreeswitch C core"]
    esl --> core
    xml["XML confdir"] --> core
    core --> sql["SQLite / ODBC / pgsql"]
    core --> rtp["RTP + libsrtp + OpenSSL DTLS"]
    core --> dso["Loadable modules .so"]
    dso --> langs["Lua / Python3 / V8 / Perl / Java"]
```

## Stack Summary

| Layer | Main technology | Why it exists in this repo |
|-------|-----------------|----------------------------|
| Signaling | Sofia-SIP (`sofia-sip-ua >= 1.13.18`), Verto + libks | SIP and HTML5/WebRTC endpoints (`mod_sofia`, `mod_verto`) |
| Runtime | C11-era C + pthreads; some C++ modules | Core, modules, Windows service |
| Media | RTP (`src/switch_rtp.c`), Cisco libSRTP 2.4.0, Opus, SpeexDSP, SpanDSP, libvpx 1.12.0 | Real-time audio/video, SRTP, fax/modem, VP8/VP9 |
| Control plane | ESL (`libs/esl`) + `mod_event_socket` | Out-of-process API; `fs_cli` |
| Config | XML + preprocessor | Dialplan, directory, profiles — not compiled constants |
| Storage | SQLite `>= 3.6.20` (core); optional PostgreSQL / MariaDB `>= 3.0.9` | Scoreboard, voicemail/CDR backends |
| Build | Autotools (autoconf `>= 2.59`, automake `>= 1.7`, libtool `>= 1.5.14`) | Unix; Windows via `Freeswitch.2017.sln` |
| Testing | FCTX macros in `src/include/test/switch_fct.h` via `switch_test.h` | `tests/unit/` Autotools programs |
| Docs PKB | Sphinx + MyST + sphinx-intl (`man/pyproject.toml`, Python `^3.10`) | This knowledge base only — not linked into `freeswitch` |

## Main Languages and Frameworks

- **Primary language**: C (core `src/switch_*.c`, most modules). Public API is `src/include/switch.h`.
- **Secondary languages**: C++ (`src/switch_cpp.cpp`, `mod_v8`, `mod_opal`, `mod_cv`, `mod_mariadb` headers); Lua (`mod_lua`, tries luajit then lua 5.3/5.2/5.1); Python 3 (`mod_python3`); JavaScript via V8 (`mod_v8`); Perl, Java, C# (`mod_managed`).
- **Primary “framework”**: FreeSWITCH module ABI (`SWITCH_MODULE_DEFINITION`, `switch_loadable_module_interface`) — not a web framework.
- **Build tool**: GNU Autotools (`bootstrap.sh` → `configure` → `make`). Historical `src/CMakeLists.txt` (CMake 2.6 comment) is not the CI path. macOS CI uses Homebrew; Linux CI uses Debian **bookworm** / **trixie** images (`.github/workflows/`).

## Key Dependencies

Minimum versions are **configure floors**. Distro packages may be newer. Out-of-tree deps are **not** in this git tree.

| Package | Area | Why it matters | Evidence |
|---------|------|----------------|----------|
| Sofia-SIP `sofia-sip-ua >= 1.13.18` | SIP | `mod_sofia`; CI clones `freeswitch/sofia-sip` | `configure.ac` |
| SpanDSP `>= 3.1.1` | DSP / fax / some codecs | `mod_spandsp`; hard error if missing | `configure.ac` |
| libks2 `>= 2.0.11` or libks `>= 1.8.2` | KS runtime | Required if `mod_verto` or `mod_signalwire` enabled | `configure.ac` |
| signalwire_client2 `>= 2.0.0` or signalwire_client `>= 1.0.0` | Cloud pairing | `mod_signalwire` | `configure.ac` |
| OpenSSL `>= 1.0.1e` | TLS / DTLS-SRTP | `SAC_OPENSSL`; `SSL_CTX_set_tlsext_use_srtp`, `DTLSv1_method` | `configure.ac` |
| SQLite `>= 3.6.20` | Core DB | `sqlite3_initialize()` in `switch_core_init` | `configure.ac`, `src/switch_core.c` |
| PCRE2 `libpcre2-8 >= 10.00` | Regex | Dialplan / XML matching | `configure.ac` |
| libcurl `>= 7.19` | HTTP | `mod_xml_curl`, `mod_http_cache`, core CURL | `configure.ac` |
| libedit `>= 2.11` | Console | CLI line editing; `--disable-core-libedit-support` to skip | `configure.ac` |
| zlib | Compression | Hard error if missing | `configure.ac` |
| libjpeg | Images | Hard error if missing | `configure.ac` |
| Speex / SpeexDSP `>= 1.2rc1` | Audio | Core codecs / AGC | `configure.ac` |
| Opus `>= 1.1` | Audio | `mod_opus` | `configure.ac` |
| libsndfile `>= 1.0.20` | Files | `mod_sndfile` | `configure.ac` |
| APR 1.2.8 (forked as `fspr`) | Portability | Bundled `libs/apr` (`fspr_version.h`) unless `SYSTEM_APR` | `libs/apr/include/fspr_version.h` |
| libSRTP **2.4.0** | SRTP | Bundled `libs/srtp` | `libs/srtp/configure.ac` `AC_INIT` |
| libvpx **1.12.0** (“Torrent Duck”) | VP8/VP9 | Bundled `libs/libvpx` | `libs/libvpx/README` |
| libyuv | Video scale/convert | Bundled `libs/libyuv` | `libs/libyuv/README.md` |
| PostgreSQL libpq | Optional DB | `mod_pgsql`; `AC_CHECK_LIB([pq])` | `configure.ac` |
| MariaDB `>= 3.0.9` | Optional DB | `mod_mariadb` | `configure.ac` |
| FFmpeg libs (avcodec `>= 53.35.0`, avformat, swscale, …) | Optional A/V | `mod_av` | `configure.ac`, `debian/control-modules` |
| Python 3 | Embedded scripts | `mod_python3`; version taken from `python3 -V` at configure | `configure.ac` |

Optional/module-only floors also exist for mpg123, shout, AMR, codec2, flite, mongoc, memcached, AMQP `librabbitmq >= 0.5.2`, pocketsphinx `>= 5`, OpenCV, VLC, ImageMagick, and others in `configure.ac` — enable the matching `modules.conf` line or the check is skipped.

## Runtime Interaction Model

1. **Signaling** (Sofia/Verto) accepts a call and asks the core for a `switch_core_session_t`.
2. **Core** runs the channel state machine; **XML dialplan** (`mod_dialplan_xml`) picks applications.
3. **Applications** (dptools, conference, …) bridge or play media; **codecs** and **libSRTP/OpenSSL** handle RTP.
4. **Events** go to the in-process bus; **ESL** clients and CDR modules consume them. **SQLite** holds core scoreboard unless `-nosql`.

Layers do **not** talk over HTTP internally. The process is the unit of composition; DSOs register interface tables.

## Build and Packaging Notes

- **Local development**: `./bootstrap.sh -j && ./configure --prefix=... && make && make install` ([Quick Start](01-quick-start.md)). Debug: `./devel-bootstrap.sh`.
- **CI**: `.github/workflows/ci.yml` (Debian bookworm-amd64 base `signalwire/freeswitch-public-ci-base`), `macos.yml` (Homebrew + `signalwire/homebrew-signalwire/{libks2,signalwire-c2,spandsp}`), `windows.yml`, `scan-build.yml` (clang-14).
- **Production / distro**: Debian packages via FSGET/FSDEB (`scripts/packaging/`); module `Build-Depends` in `debian/control-modules` (Bookworm vs Trixie ffmpeg package names differ). Docker packaged images need a SignalWire token (`docker/master/Dockerfile`); source image recipe is `docker/examples/Debian11/Dockerfile`.
- **Windows**: Visual Studio 2017 solution `Freeswitch.2017.sln`, projects under `w32/`.

## Docs tooling (not in the switch binary)

`man/pyproject.toml` / `man/requirements.txt`: Python `^3.10`, Sphinx `>=7`, myst-parser, sphinx-rtd-theme, sphinx-intl, sphinxcontrib-mermaid. Used only to build this PKB.

## Common Misconceptions

| Misconception | Reality in this repo |
|---------------|----------------------|
| FreeSWITCH is a Node/Java/Go service | It is a C process with optional language modules |
| Sofia-SIP / SpanDSP / libks live in `libs/` | They are **external**; CI and Docker clone and install them first |
| `src/CMakeLists.txt` is how you build | Autotools is the supported Unix path; CMake file is historical |
| `man/pyproject.toml` is a product dependency | It is Sphinx for documentation only |
| `build/` is a CMake output directory | It is Autotools helper sources (`modules.conf.in`, config macros) |
| SQLite is optional for a default core | Core calls `sqlite3_initialize()` at init; `-nosql` only disables the internal SQL scoreboard |
| One `modules.conf` | Build-time `modules.conf` vs runtime `autoload_configs/modules.conf.xml` ([Architecture](02-architecture.md)) |

## Related Documentation

- [Architecture](02-architecture.md)
- [Repository Map](04-repo-map.md)
- [Quick Start](01-quick-start.md)
- [Build](08-build.md)

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
