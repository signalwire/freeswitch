# Architecture Decision Records (ADR)

This directory contains Architecture Decision Records (ADRs) documenting significant architectural decisions made in this project.

## What is an ADR?

An Architecture Decision Record (ADR) is a document that captures an important architectural decision made along with its context and consequences.

## ADR Format

Each ADR follows this structure:

- **Status**: Proposed, Accepted, Deprecated, or Superseded
- **Context**: The circumstances and constraints
- **Decision**: What was decided
- **Alternatives**: Other options considered
- **Consequences**: The results of the decision (positive, negative, neutral)

## How to Create an ADR

Use the `/PKB-adr` command:

```bash
/PKB-adr "Use PostgreSQL over MongoDB"
```

## ADR List

There are no numbered records yet (`adr/0001-*.md`). Create one with `/PKB-adr "title"`, then add the new file to the toctree below (or restore a `:glob:` entry `0*`).

```{toctree}
:maxdepth: 1
:caption: ADR documents

template
```

## Index by Status

### Accepted

<!-- List accepted ADRs here -->

### Proposed

<!-- List proposed ADRs here -->

### Deprecated

<!-- List deprecated ADRs here -->

## References

- [ADR GitHub Organization](https://adr.github.io/)
- [Documenting Architecture Decisions](https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions)

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
