# Appendix-04: Documentation Changelog

<!-- maintained-by: human+ai -->

Tracks Project Knowledge Base changes (page additions, translations, tooling). Code and feature changes belong in the repository `docs/ChangeLog` and upstream release notes, not here.

## 2026-08-17

### Changed

- Aligned PKB with the [FreeSWITCH Users Manual](https://developer.signalwire.com/freeswitch/) as the operator configuration source of truth (Explained/Confluence remain historical)
- Added PKB ↔ Users Manual page map on `index.md`; three configuration domains (directory / dialplan / configuration) on overview and architecture
- Extended Quick Start with configuration-root table, vanilla first-call numbers (`9196` echo, `1000`–`1019`), and `default_password=1234` (verified in `vars.xml`)
- Documented `$${var}` vs `${var}`, `user_context` vs SIP profile `context`, `pre_load_modules.conf.xml`, compiled XML in `log_dir`, and media late-negotiation / bypass / proxy flags
- Glossary, FAQ, runbook, AI guide, and documentation-process pages now point at Users Manual chapters instead of treating Explained as primary
- Refreshed `locale/zh_CN/LC_MESSAGES/` for the Users Manual alignment (new strings + unfuzzy of updated paragraphs)

### Added

- Added zh_CN gettext catalogs under `locale/zh_CN/LC_MESSAGES/` (all numbered pages, appendices, ADR/change templates)
- Filled `appendix-01-faq.md` and `appendix-02-glossary.md` (install, two `modules.conf`, ESL bind vs `fs_cli`, 25 terms)
- Filled `12-document.md` (PKB `man/` source of truth, Level 1–3, zh_CN catalogs)
- Filled `07-conventions.md` (`.clang-format`, SubmittingPatches, two `modules.conf`, secrets)
- Filled `11-observability.md` (`switch_log` levels, `freeswitch.log`, HEARTBEAT/`status`; no Prometheus)
- Filled `06-workflows.md` (inbound SIP, REGISTER, ESL originate, `reloadxml`)
- Filled `10-runbook.md` (start/stop/`fs_cli`, inspect `$PREFIX`, 12 common issues)
- Filled `09-testing.md` (`tests/unit/run-tests.sh`, `ci.sh`, ESL `make check`, PKB `make html-en`)
- Filled `08-build.md` (Autotools, GHA matrix, FSDEB/tarball, PKB Sphinx publish)
- Filled `05-data-and-api.md` (XML/SQL model, ESL, Verto JSON-RPC, core events; extractor found no REST)
- Filled `03-tech-stack.md` from `configure.ac` floors plus bundled lib versions (`gen_tech_stack.py` only saw PKB Sphinx)
- Filled `02-architecture.md` (C4 Context→Code, SIP session flow, module vs autoload)
- Filled `01-quick-start.md` from bootstrap/CI/Docker/packaging commands
- Filled `04-repo-map.md` from `gen_repo_map.sh` plus verified `src/mod/` category map
- Filled `00-overview.md` from repo facts (purpose, users, C4 context, scope)
- Initial bilingual Sphinx PKB skeleton under `man/` (`/PKB-init --sphinx --bilingual=zh_CN`)
- Standard numbered pages `00`–`12`, appendices, ADR/change templates, and helper scripts
- Language switcher scaffold for English and `zh_CN`

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
