# AGENTS.md - FreeSWITCH

<!-- Follows https://agents.md. Keep this file short: target <100 lines. -->

Modular C softswitch: sessions, media, XML dialplan, and in-process
loadable modules. This file is the operating map, not the manual.

Read this before editing. Deep architecture, workflows, and runbooks
stay in `man/` and the [Users Manual](https://developer.signalwire.com/freeswitch/).

## Context Map

- README: [`README.md`](README.md) — product pointer and package/build links
- PKB: [`man/index.md`](man/index.md) — start here; do not copy its content. Next: `man/01-quick-start.md`, `04-repo-map.md`, `07-conventions.md`, `10-runbook.md`
- Patches: [`docs/SubmittingPatches`](docs/SubmittingPatches)
- Security: [`SECURITY.md`](SECURITY.md)
- ADRs: [`man/adr/`](man/adr/index.md)

Repo layout:

- `src/` — core (`src/switch.c` is `main`); public API `src/include/switch.h`
- `src/mod/<category>/mod_<name>/` — loadable modules
- `conf/` — XML profiles; `conf/vanilla` is a demo PBX, not production
- `libs/` — bundled APR, SRTP, ESL, VPX; Sofia-SIP, SpanDSP, and libks are **out of tree**
- `tests/unit/` — libfreeswitch unit tests
- `man/` — Sphinx PKB; `docs/man/` is Unix man pages, not the PKB
- `build/modules.conf.in` — **build-time** compile list (copied to `modules.conf`)

## Commands

Sofia-SIP, libks, SpanDSP, and signalwire-c must already be installed —
they are not in this git tree. Details: `man/01-quick-start.md`.

```bash
./bootstrap.sh -j
./configure --prefix="$HOME/fs"
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
make install                                          # samples-conf if confdir is empty
"$HOME/fs/bin/freeswitch" -ncwait -nonat
"$HOME/fs/bin/fs_cli" -x status                       # working: a line starting with UP
cd tests/unit && ./run-tests.sh                       # after configure + make + make install
make -C libs/esl check                                # ESL tests; CI group 1
cd man && poetry install && make html-en              # PKB Sphinx only, not product deps
```

There is no tree-wide lint CI gate. Match surrounding C (tabs, `.clang-format`);
do not reformat unrelated lines.

## Harness Rules

- Never fabricate paths, APIs, commands, tests, or results; inspect the
  repo or run the command first.
- Ask when ambiguity changes the output; otherwise resolve uncertainty
  by reading files and existing patterns.
- Think before coding: state assumptions, tradeoffs, and success
  criteria before non-trivial edits.
- Keep it simple: solve the requested problem without speculative
  features, one-off abstractions, or future-proofing.
- Make surgical changes: every changed line should trace to the request;
  leave unrelated code and formatting alone.
- Verify before reporting done; a plausible diff is not proof.

## Project Rules

- Do not mix `modules.conf` (build: `src/mod/...` lines) with
  `modules.conf.xml` (runtime: `<load module="mod_…"/>`) — a compiled
  module can still be unloaded.
- Do not edit `freeswitch.xml.fsxml` while the process is running — it
  is the memory-mapped compiled XML under `log_dir`.
- Do not add files to `conf/vanilla` for a new module —
  `docs/SubmittingPatches` forbids it.
- Use `$${name}` only for preprocessor vars (`vars.xml`); `${name}` is
  a call-time channel variable — mixing them is a common config bug.
- New core tests must use `src/include/test/switch_test.h`
  (`tests/unit/README`).
- Do not commit SignalWire PATs, production SIP/ESL passwords, or TLS
  keys. Vanilla `ClueCon` / `default_password=1234` are demo values.
- Report vulnerabilities to `security@signalwire.com` (`SECURITY.md`);
  do not open a public issue for an undisclosed vuln.
- Do not treat `man/pyproject.toml` as FreeSWITCH runtime dependencies.

## AI Tooling

Primary tools: Cursor and other AGENTS.md-aware clients. No
`CLAUDE.md` / `GEMINI.md` symlinks unless you ask for them.

## Keeping Current

Update this file when commands, layout, guardrails, or linked docs
move. If `man/` changes, update links here instead of copying content.
After a user correction, add or tighten one concrete rule, then prune.

<!-- last_updated: 2026-08-18 -->
