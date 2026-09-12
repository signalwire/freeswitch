# FreeSWITCH Knowledge Base

<!-- maintained-by: human+ai -->

This documentation is the Project Knowledge Base for FreeSWITCH. It is designed to help both humans and AI locate code, understand architecture, execute workflows, and verify changes.

## Overview

```{admonition} Purpose
:class: tip

This PKB should help readers:
- **Locate** code, configs, scripts, and entry points in **this git tree**
- **Understand** architecture, stack choices, and workflows
- **Execute** setup, build, release, and debugging tasks
- **Verify** behavior with tests, observability, and documentation checks
```

Operator configuration (XML, SIP profiles, dialplan, directory, codecs, ESL programming) is documented in the [FreeSWITCH Users Manual](https://developer.signalwire.com/freeswitch/). This PKB does not replace that manual. It maps the same concepts onto verified paths in this repository.

| Layer | What it is | Start here |
|-------|------------|------------|
| **Users Manual** | Operator configuration reference. Parameters and defaults are verified against FreeSWITCH source and the shipped vanilla config. | [developer.signalwire.com/freeswitch](https://developer.signalwire.com/freeswitch/) |
| **This PKB** | Repo map, C4 architecture, Autotools/CI, ESL/XML contracts as implemented here, runbook for this clone | [Project Overview](00-overview.md) |
| **Historical wiki** | Older tutorials and Confluence pages still linked from `README.md` | [FreeSWITCH Explained](https://developer.signalwire.com/freeswitch/FreeSWITCH-Explained/), [Confluence](https://freeswitch.org/confluence/) |

```{admonition} How to use both
:class: note

Read [Users Manual Part 1](https://developer.signalwire.com/freeswitch/foundations/introduction) and [Part 2](https://developer.signalwire.com/freeswitch/configuration/xml) for the configuration model. Use this PKB when you need the C file, Autotools target, or CI path that implements that model.
```

## Table of Contents

```{toctree}
:maxdepth: 2
:caption: Getting Started

00-overview
01-quick-start
```

```{toctree}
:maxdepth: 2
:caption: Design & Structure

02-architecture
03-tech-stack
04-repo-map
05-data-and-api
06-workflows
```

```{toctree}
:maxdepth: 2
:caption: Development

07-conventions
08-build
09-testing
```

```{toctree}
:maxdepth: 2
:caption: Operations

10-runbook
11-observability
12-document
```

```{toctree}
:maxdepth: 1
:caption: Appendix

appendix-01-faq
appendix-02-glossary
diagrams-guide
CHANGELOG
ai-guide
adr/index
changes/index
```

## PKB page to Users Manual

Keep PKB numbering stable. When an operator question is about *how to configure* FreeSWITCH, follow the Users Manual chapter; when it is about *where it lives in this tree*, stay on the PKB page.

| PKB page | Users Manual | Use the PKB for |
|----------|--------------|-----------------|
| [00 Overview](00-overview.md) | [Ch 1 Core Concepts](https://developer.signalwire.com/freeswitch/foundations/introduction) | Scope of this clone, C4 context, edition vs cloud |
| [01 Quick Start](01-quick-start.md) | [Ch 2 Getting Started](https://developer.signalwire.com/freeswitch/foundations/getting-started) | Source-build from this tree; Sofia-SIP / libks out of tree |
| [02 Architecture](02-architecture.md) | Ch 1 + [Ch 3 XML config](https://developer.signalwire.com/freeswitch/configuration/xml) + [Ch 5 Module loading](https://developer.signalwire.com/freeswitch/configuration/module-loading/) | Session state machine, module loader, in-process DSOs |
| [04 Repo map](04-repo-map.md) | Config paths relative to the directory that contains `freeswitch.xml` | `src/`, `src/mod/`, `conf/vanilla/` ownership |
| [05 Data and API](05-data-and-api.md) | [Ch 6 Directory](https://developer.signalwire.com/freeswitch/users-and-endpoints/user-directory), [Ch 12 XML Dialplan](https://developer.signalwire.com/freeswitch/dialplan/xml), [Ch 46 Inbound ESL](https://developer.signalwire.com/freeswitch/programming/esl-inbound) | Event ids, SQL scoreboard, ESL command table in this tree |
| [06 Workflows](06-workflows.md) | Ch 1 anatomy of a call; [Ch 7 SIP profiles](https://developer.signalwire.com/freeswitch/users-and-endpoints/sip-profiles) | INVITE/REGISTER/originate traces with C symbols |
| [10 Runbook](10-runbook.md) | Ch 2 + Part 11 Troubleshooting | Prefix vs FHS paths, `fs_cli` against this install |
| [11 Observability](11-observability.md) | Part 11 diagnostics | `switch_log` levels, HEARTBEAT; no Prometheus in-tree |

Parts 6–10 of the Users Manual (applications, integration, module reference, recipes) have no extra PKB chapter. Link those from [Workflows](06-workflows.md) and [Data and API](05-data-and-api.md) when an operator needs parameter tables.

## Quick Start

### For New Developers

1. Read [Project Overview](00-overview.md) to understand purpose, users, and scope.
2. Follow [Quick Start](01-quick-start.md) to get the project running locally.
3. Study [Repository Map](04-repo-map.md) to locate entry points and major directories.
4. Review [Runbook](10-runbook.md) and [Conventions](07-conventions.md) before making changes.

### For AI Assistants

Use a layered reading order:

1. **Round 1**: read `00-overview`, `02-architecture`, and `04-repo-map` (plus Users Manual Ch 1 if the three configuration domains are new)
2. **Round 2**: read `05-data-and-api` and `06-workflows`
3. **Round 3**: read `09-testing`, `10-runbook`, `11-observability`, and `12-document`

See [How to Use This Documentation for AI](ai-guide.md) for the detailed workflow.

## Key Concepts

:::::{grid} 2
:gutter: 3

::::{grid-item-card} Navigation
:link: 04-repo-map
:link-type: doc

Find the project structure, startup files, and major module boundaries.
::::

::::{grid-item-card} Architecture
:link: 02-architecture
:link-type: doc

Learn how the system is decomposed and how the main pieces interact.
::::

::::{grid-item-card} Workflows
:link: 06-workflows
:link-type: doc

Explore critical request, event, and business flows with code references.
::::

::::{grid-item-card} Testing
:link: 09-testing
:link-type: doc

Review the test layers, critical regressions, and validation commands.
::::

:::::

## Documentation Maintenance

```{admonition} Living Documentation
:class: important

Keep the PKB fresh without wasting tokens:
- run rule-based freshness checks before LLM-heavy doc work
- apply mechanical Level 1 updates first
- batch Level 2 refreshes by PR, sprint, or milestone
- keep ADRs and product rationale human-led
```

## Contributing

See [Conventions](07-conventions.md) for coding standards and [Documentation Process](12-document.md) for PKB maintenance rules.

## Indices and tables

- {ref}`genindex`
- {ref}`search`

---

**Version**: {sub-ref}`release`
**Last Updated**: {sub-ref}`today`
<!-- PKB-metadata
last_updated: 2026-08-17
commit: d94936cc10
updated_by: human+ai
review_status: pending
review_score: 0
reviewed_by:
confidentiality: L1
-->
