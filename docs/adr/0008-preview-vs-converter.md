# ADR-0008: FileFormat Preview Path, Converter Production Path

## Status

Accepted for Milestone 0. The converter arrives at Milestone 7.

## Decision

Two entry points with different cost contracts:

```text
raster-geotiff      preview / interactive / inspection
usd-raster-convert  long-running / deterministic / production
```

The FileFormat Plugin enforces an interactive ceiling — a maximum authored
vertex count — and fails with an actionable diagnostic when a request exceeds
it. The converter has no such ceiling and owns payload generation, manifests,
and generated-cache population.

The plugin **reuses** committed cache entries; it does not generate them.

## Rationale

`SdfFileFormat::Read` is called on a host thread during composition. A host
usually cannot cancel it meaningfully, cannot report progress through it, and
may call it during an interaction the user expects to be immediate. A read that
takes four minutes is indistinguishable from a hang.

Raster conversion has an enormous cost range: the same code path serves a 2x2
fixture and a 40000x40000 DEM. Without an explicit split, either the plugin
becomes unusable on large data or the pipeline becomes unusable for production
because everything is capped.

Splitting the paths lets each be honest:

- The plugin optimizes for time-to-first-pixel: metadata by default, decimated
  previews, bounded windows.
- The converter optimizes for throughput and determinism: bounded memory,
  resumability, byte-stable output, manifests.

A diagnostic that names the converter and the arguments that would fit is more
useful than either a hang or a silently degraded result. Silently downgrading
`lod=quality` to `preview` would be the worst option: the user would believe
they were looking at full-resolution terrain.

## Consequences

- The ceiling is a published number in the plugin README and the capability
  matrix, not an implementation detail.
- The converter is a first-class deliverable with its own milestone, not a
  test utility.
- Cache generation lives on the converter side, so an interactive read never
  starts a long write. See [CACHE.md](../architecture/CACHE.md).
- Both paths use the same libraries. The split is in orchestration and policy,
  never in decoding or authoring code, so a behavior difference between preview
  and production is a bug.
- Equivalence tests assert that converter output at `lod=off` and a plugin read
  of a small fixture at `lod=off` produce identical authored geometry.

## Related

- [PLUGIN_ADAPTER.md](../architecture/PLUGIN_ADAPTER.md) — the ceiling and the
  adapter contract.
- [TILING.md](../architecture/TILING.md) — what the converter produces.
- [ADR-0003](0003-windowed-raster-reader.md) — why bounded reads make both
  paths possible.
