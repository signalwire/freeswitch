# 08. Build, Release, and Publish

<!-- maintained-by: human+ai -->

Local Autotools/Windows builds, GitHub Actions, Debian/Docker packaging, version fields, and PKB HTML. First-run commands live in [Quick Start](01-quick-start.md); this page is the pipeline map.

## Scope

- Unix source build (`bootstrap.sh` → `configure` → `make` → `make install`)
- Per-module rebuild, install layout, sounds
- GitHub Actions (unit tests, scan-build, Debian matrix, macOS, Windows, tarball)
- Packaged install (FSGET) and `.deb` build (FSDEB)
- Version strings and release branches
- PKB Sphinx bilingual build / AgentBox publish

Upstream operator docs: [FreeSWITCH Users Manual](https://developer.signalwire.com/freeswitch/) ([Ch 2 Getting Started](https://developer.signalwire.com/freeswitch/foundations/getting-started)). Package downloads: [files.freeswitch.org](https://files.freeswitch.org/releases/freeswitch/). Historical wiki: [FreeSWITCH Explained](https://developer.signalwire.com/freeswitch/FreeSWITCH-Explained/).

## Local Build Paths

### Development (Unix / macOS)

Prerequisites: Autotools floors in `scripts/ci/build-requirements.sh` (autoconf `>= 2.59`, automake `>= 1.7`, libtool `>= 1.5.14`) plus Sofia-SIP, SpanDSP, libks, signalwire-c as in [Tech Stack](03-tech-stack.md).

```bash
./bootstrap.sh -j          # copies build/modules.conf.in → modules.conf if missing
# edit modules.conf to enable/disable src/mod/... trees, then:
./configure --prefix="$HOME/fs"
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
make install
```

Debug CFLAGS (`-ggdb3 -O0`): `./devel-bootstrap.sh` then the same `configure`/`make`.

Rebuild one module after `modules.conf` is already in place:

```bash
make mod_sofia
make mod_sofia-install
```

(Top-level `Makefile.am` dispatches `mod_*` into `src/mod`.)

ASAN unit-test configure used by CI:

```bash
./ci.sh -t unit-test -a configure -c freeswitch
./ci.sh -t unit-test -a build -c freeswitch
./ci.sh -t unit-test -a install -c freeswitch
```

(`ci.sh` also builds Sofia-SIP when `-c sofia-sip`.)

### Production / prefix install

Same `configure && make && make install`. Default prefix is `/usr/local/freeswitch` (`AC_PREFIX_DEFAULT` in `configure.ac`). FHS layout (`--enable-fhs` when prefix is set) puts modules under `${libdir}/freeswitch/mod`.

Debian systemd unit (`debian/freeswitch-systemd.freeswitch.service`) starts:

```text
/usr/bin/freeswitch -u freeswitch -g freeswitch -ncwait -nonat
```

Sounds are **not** in this git tree. `make sounds` / `make sounds-install` downloads Callie 8 kHz packages (`Makefile.am`, default `en-us-callie-8000`). Specs at repo root: `freeswitch-sounds-*.spec`.

### Windows

```text
msbuild Freeswitch.2017.sln -t:build -verbosity:minimal -property:Configuration=Release -property:Platform=x64
```

Helper: `msbuild.cmd` (locates VS via `vswhere.exe`). Projects under `w32/`. CI uploads `x64\*.msi` on master / v1.10 / v1.11 (or PR titles containing `:upload-artifacts`).

### Expected outputs (Unix prefix)

| Path | Content |
|------|---------|
| `$prefix/bin/` | `freeswitch`, `fs_cli`, `fs_encode`, `fs_tts`, `fs_ivrd` (`bin_PROGRAMS` in `Makefile.am`) |
| `$prefix/mod/` | Loadable `.so` / `.dylib` / `.dll` |
| `$prefix/conf/` | Vanilla samples if the dir did not exist (`samples-conf`) |
| `$prefix/db/`, `log/`, `run/`, `scripts/`, `htdocs/` | Runtime dirs (`install-data-local`) |
| `src/include/switch_version.h` | Generated from `switch_version.h.template` |

## CI/CD Pipelines

All under `.github/workflows/`. Linux jobs use `signalwire/freeswitch-public-ci-base:bookworm-amd64` unless noted.

| Workflow | Trigger | Purpose | Key output |
|----------|---------|---------|------------|
| `ci.yml` | push `master` / `v1.10` / `v1.11`; PR open/sync; `workflow_dispatch` | Unit tests in 2 groups (`unit-test.yml` via `ci.sh`); optional DinD (`unit-test-dind.yml`); clang `scan-build.yml` | Pass/fail; scan-build reports |
| `build.yml` (“Build and Distribute”) | PR; push `master` / `v1.10` / `v1.11`; `workflow_dispatch` | Debian **bookworm** + **trixie** × amd64 / arm32v7 / arm64v8. PR excludes some ARM combos. Branch `v1.10`/`v1.11` → `release`; `master` → `unstable` | `.deb` / `.dsc` / `.changes` / tarballs; upload when push is on `signalwire/freeswitch` |
| `macos.yml` | push/PR those branches; `workflow_dispatch` | Homebrew deps + `./bootstrap.sh -j` + `./configure --prefix=OUT` + `make install` | Artifact `freeswitch-macos-build` |
| `windows.yml` | PR; push `master` / `release` | MSBuild x64 Release | MSI artifact |
| `tarball.yml` | push `v1.10` / `v1.11`; `workflow_dispatch` | `scripts/ci/src_tarball.sh` | `.tar.gz` / `.bz2` / `.xz` / `.zip` + checksums under `src_dist/`; Teleport upload on branch push |
| `unit-test.yml` | `workflow_call` from `ci.yml` | Configure/build/install Sofia-SIP + FreeSWITCH; `tests/unit/run-tests.sh`; `libs/esl` `make check` on group 1 | Logs on failure |

`build.yml` issues a temporary repo token (`repo-auth-client`) before the public Debian Docker matrix (`.github/docker/debian/<codename>/<arch>/public.<release>.Dockerfile`).

## Release Process

Version fields that **must stay together** (`configure.ac` comment):

1. `AC_INIT([freeswitch], [1.11.3-dev], …)`
2. `SWITCH_VERSION_MAJOR` / `MINOR` / `MICRO` (and optional `REVISION` / `REVISION_HUMAN` for a tagged release)
3. `build/next-release.txt` (currently `1.11.3-dev`) — `src_tarball.sh` reads this and writes `.version`

Then:

1. Land changes via GitHub PR (`docs/SubmittingPatches`, `README.md`). Topic branches; `./scripts/setup-git.sh` for author identity.
2. CI green on `ci.yml` + platform builds you care about.
3. For a packaged line: merge/tag on `v1.10` or `v1.11` so `build.yml` treats `release=release`. `master` publishes **unstable**.
4. Source archives: `tarball.yml` or `./scripts/ci/src_tarball.sh` (needs a **git** tree; FSDEB same constraint — `scripts/packaging/build/README.md`).
5. Debian packages from git: FSDEB  
   `curl -sSL https://freeswitch.org/fsdeb | bash -s -- -b BUILD_NUMBER -o OUT_DIR -w /path/to/freeswitch`
6. Operator install from SignalWire repo: FSGET (`scripts/packaging/README.md`) — requires a PAT.
7. Verify: `freeswitch -version` / `fs_cli -x version`; package lists under `freeswitch.signalwire.com` (workflow `REPO_DOMAIN`).

Human product release notes: [Confluence Release Notes](https://freeswitch.org/confluence/display/FREESWITCH/Release+Notes). Mechanical git draft: `cd man && make changelog-draft` (writes `_generated/`; slow on this history).

## Documentation Build and Publish

PKB lives in `man/` (not `docs/man` Unix pages).

```bash
cd man
poetry install
make html-en          # _build/site/en/
make gettext
make intl-update      # locale/zh_CN/LC_MESSAGES/
make html-all         # en + zh + landing
make serve            # http://127.0.0.1:8000/en/ and /zh/ (needs both trees)
# make serve-watch    # English live reload only; do not click 中文 there
```

AgentBox (optional): `make publish` runs `agentbox-init` → `html-all` → `strip-confidential` (default strip `L3+`) → `gen_agentbox_files.py` → `abx func deploy freeswitch-doc`. Function slug is `freeswitch-doc` (`agentbox.yaml`).

## Common Failures

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `bootstrap.sh` dies on autoconf/libtool | Tool older than `scripts/ci/build-requirements.sh` | Install GNU autotools; macOS Homebrew `autoconf automake libtool` |
| Link/configure missing Sofia, KS, SpanDSP, signalwire | Out-of-tree deps | Follow `docker/examples/Debian11/Dockerfile` or `ci.sh -c sofia-sip` |
| Module not in `mod/` after install | Commented in `modules.conf` **or** not in runtime `modules.conf.xml` | Uncomment, `make mod_*`, and load XML |
| `make dist` / FSDEB from a tarball | Scripts require git | Clone the repo (`scripts/packaging/build/README.md`) |
| FSGET / `docker/master` apt 401 | No SignalWire PAT | PAT or source build |
| Debian ffmpeg headers differ | Bookworm vs Trixie package names | `debian/control-modules` `Build-Depends-Bookworm` / `Build-Depends-Trixie` |
| Windows MSI missing in CI | Not master/v1.10/v1.11 and PR title lacks `:upload-artifacts` | See `windows.yml` `if:` |
| `scan-build-14` not found | Local machine lacks clang-14 analyzer | Use CI image or install clang tools (`ci.sh` scan-build path) |
| PKB `make html` fails | Poetry env not installed; missing `linkify-it-py` | `cd man && poetry install` |
| Changelog draft hangs | `gen_changelog.sh` walks a large git log | Run with `--range` (e.g. `HEAD~20..HEAD`) |
| Clicking 中文 404s `http://127.0.0.1:8000/zh/` | `make serve-watch` (old `make serve`) serves English `_build/html`; the switcher expects `_build/site/{en,zh}` | Stop that process; `make serve` or `make serve-all` |

## Related Documentation

- [Quick Start](01-quick-start.md)
- [Tech Stack](03-tech-stack.md)
- [Testing](09-testing.md)
- [Runbook](10-runbook.md)
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
