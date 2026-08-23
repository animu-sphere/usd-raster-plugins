# OST Reports

These reports are append-only records of OpenStrata (`ost`) adoption in this
repository. They preserve the commands that were run, the CI evidence that was
observed, repository-side fixes, and any follow-up asks for OpenStrata.

## Reading Order

| Report | Date | Subject | OST version | Result |
| --- | --- | --- | --- | --- |
| [01](01-2026-08-23-v0.22.2-workspace-cells-bundle-free-repository.md) | 2026-08-23 | `kind: workspace` cells in a repository whose libraries land before its first bundle | 0.22.2 local dogfooding | `ost build` and `ost test` pass on the pinned runtime; the generated workflow's dependency-graph rung fails closed with no bundle present, so it is held until milestone 2. One P2 upstream ask |

Reports are historical evidence. When a later OpenStrata version changes an
observation, add a new report rather than rewriting the old one.

## Open asks

| Report | Priority | Ask | State |
| --- | --- | --- | --- |
| [01](01-2026-08-23-v0.22.2-workspace-cells-bundle-free-repository.md) | P2 | Let `ost plugin test --workspace --graph-only` succeed on a workspace with zero bundles | open against `ost 0.22.2` |
