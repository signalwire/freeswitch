# Appendix-03: Diagram Quick Reference

<!-- maintained-by: human+ai -->

Mermaid is the primary diagramming tool for PKB documentation. This page covers the most common diagram types. For additional examples (class diagrams, ER diagrams, Gantt charts), refer to the Mermaid documentation at <https://mermaid.js.org/intro/>.

## Flowcharts

````markdown
```{mermaid}
flowchart TD
    Start([Start]) --> Input[/User Input/]
    Input --> Process[Process Data]
    Process --> Decision{Valid?}
    Decision -->|Yes| Success[Success]
    Decision -->|No| Error[Error]
    Error --> Input
    Success --> End([End])
```
````

## Sequence Diagrams

````markdown
```{mermaid}
sequenceDiagram
    participant Client
    participant API
    participant Service
    participant DB

    Client->>API: Request
    activate API
    API->>Service: Process
    activate Service
    Service->>DB: Query
    DB-->>Service: Result
    deactivate Service
    Service-->>API: Response
    API-->>Client: Response
    deactivate API
```
````

## C4 Context Diagram

````markdown
```{mermaid}
flowchart TD
    user["User"] --> system["System Under Documentation"]
    system --> external["External Dependency"]
    system --> db[("Database")]
```
````

## State Diagrams

````markdown
```{mermaid}
stateDiagram-v2
    [*] --> Draft
    Draft --> Submitted: submit()
    Submitted --> Approved: approve()
    Submitted --> Rejected: reject()
    Approved --> Published: publish()
    Rejected --> Draft: revise()
    Published --> [*]
```
````

## Tips

- Use `flowchart TD` (top-down) or `flowchart LR` (left-right) for architecture and process diagrams.
- Use `sequenceDiagram` for runtime call chains and API interactions.
- Use `stateDiagram-v2` for lifecycle and state machine documentation.
- Prefer Mermaid over PlantUML -- it renders natively in Sphinx via `sphinxcontrib-mermaid`.
- Keep diagrams focused: one concept per diagram, not the entire system.

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
