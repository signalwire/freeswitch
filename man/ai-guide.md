# 13. How to Use This Documentation for AI

<!-- maintained-by: human+ai -->

This guide explains how AI assistants should consume and use the Project Knowledge Base.

## Recommended Reading Order

1. Start with [Project Overview](00-overview.md) for purpose, scope, and users. If the three configuration domains (directory / dialplan / configuration) are new, also read [Users Manual Chapter 1](https://developer.signalwire.com/freeswitch/foundations/introduction).
2. Read [Repository Map](04-repo-map.md) to locate entry points and important directories.
3. Read [Architecture](02-architecture.md) and [Workflows](06-workflows.md) for cross-cutting behavior.
4. Check [Data and API](05-data-and-api.md) before changing schemas, contracts, or shared models.
5. Use [Testing](09-testing.md), [Runbook](10-runbook.md), and [Observability](11-observability.md) before validating or debugging changes.
6. Use [Build](08-build.md) when the change affects CI, packaging, release, or docs publishing.
7. Use [Documentation Process](12-document.md) when refreshing the PKB itself.

## The Three-Round Learning Process

### Round 1: Build the mental model

Read:

- [Project Overview](00-overview.md)
- [Architecture](02-architecture.md)
- [Repository Map](04-repo-map.md)

Expected output:

1. A short system summary
2. A map of entry points and high-change modules
3. A list of missing knowledge that blocks safe implementation

### Round 2: Trace critical flows

Read:

- [Workflows](06-workflows.md)
- [Data and API](05-data-and-api.md)
- relevant code files referenced by those docs

Expected output:

1. End-to-end call chains for 2-3 critical flows
2. Boundary conditions and likely failure modes
3. A focused validation plan

### Round 3: Prepare production-safe changes

Read:

- [Testing](09-testing.md)
- [Runbook](10-runbook.md)
- [Observability](11-observability.md)
- [ADRs](adr/index.md) when design rationale matters

Expected output:

1. A contained implementation plan
2. Validation and rollback steps
3. Any documentation that must change with the code

## When to use the Users Manual vs this PKB

| Question | Read |
|----------|------|
| What does this XML parameter mean? Default? Allowed values? | [Users Manual](https://developer.signalwire.com/freeswitch/) (Parts 1–8, Part 9 module reference) |
| Where is that implemented in **this clone**? | PKB [Repo map](04-repo-map.md), [Architecture](02-architecture.md), code references on the page |
| How do I build/test/CI this tree? | PKB [Quick Start](01-quick-start.md), [Build](08-build.md), [Testing](09-testing.md) |
| How do I write an ESL client? | Users Manual [Ch 46 Inbound ESL](https://developer.signalwire.com/freeswitch/programming/esl-inbound) plus PKB [Data and API](05-data-and-api.md) and `libs/esl/` |
| Historical wiki / old recipes | [FreeSWITCH Explained](https://developer.signalwire.com/freeswitch/FreeSWITCH-Explained/) |

Do not paste Users Manual parameter tables into PKB pages. Link the chapter; keep PKB facts verified against this tree (file paths, C symbols, Autotools).

## Core Capabilities

### Locate

Use [Repository Map](04-repo-map.md) to find:

- startup files
- routers, handlers, and command boundaries
- scripts, tests, and release config

### Understand

Use [Architecture](02-architecture.md), [Tech Stack](03-tech-stack.md), and [Workflows](06-workflows.md) to understand:

- why the system is shaped this way
- how the main layers interact
- where important trade-offs live

### Execute

Use [Quick Start](01-quick-start.md), [Build](08-build.md), and [Runbook](10-runbook.md) to:

- start the project locally
- run builds and tests
- inspect state and debug failures

### Verify

Use [Testing](09-testing.md) and [Observability](11-observability.md) to:

- choose the right validation depth
- inspect logs, metrics, traces, and alerts
- confirm whether a fix really addressed the issue

## When Updating Docs

Before refreshing PKB pages:

1. Read [Documentation Process](12-document.md).
2. Prefer rule-based freshness checks first.
3. Apply zero-token Level 1 updates before asking the LLM to rewrite prose.
4. Give the LLM only the affected PKB pages, relevant diff, and source/config snippets.
5. Keep ADR rationale and product intent human-led.

## Change Checklist

Before proposing a change:

- Have you read the relevant PKB pages?
- Have you checked related [ADRs](adr/index.md)?
- Does the change follow [Conventions](07-conventions.md)?
- Do [Testing](09-testing.md) and [Runbook](10-runbook.md) suggest extra validation?
- Should [12-document.md](12-document.md) or other PKB pages be refreshed too?

## Example Workflows

### Answering "How does authentication work?"

1. Read [Architecture](02-architecture.md).
2. Read the relevant section in [Workflows](06-workflows.md).
3. Check [Data and API](05-data-and-api.md) for contracts and models.
4. Read the code references cited by the PKB.
5. Use [Runbook](10-runbook.md) and [Observability](11-observability.md) for failure analysis.

### Implementing a new feature

1. Read [Project Overview](00-overview.md) for scope.
2. Read [Architecture](02-architecture.md) and [Tech Stack](03-tech-stack.md).
3. Trace affected flows in [Workflows](06-workflows.md).
4. Plan validation using [Testing](09-testing.md).
5. Update relevant PKB pages after the change.

### Debugging an issue

1. Check [Runbook](10-runbook.md) for common issues and inspection commands.
2. Check [Observability](11-observability.md) for logs, metrics, and diagnostic signals.
3. Re-read the affected flow in [Workflows](06-workflows.md).
4. Validate the fix against [Testing](09-testing.md).

## Continuous Improvement

When the PKB is unclear or stale:

- report the gap explicitly
- propose the smallest doc refresh that resolves the ambiguity
- prefer targeted updates over large, generic rewrites

---

**Remember**: the goal is not to memorize everything. The goal is to know where trustworthy information lives, what still needs human input, and how to keep context small enough for accurate AI work.
<!-- PKB-metadata
last_updated: 2026-08-17
commit: d94936cc10
updated_by: human+ai
review_status: pending
review_score: 0
reviewed_by:
confidentiality: L1
-->
