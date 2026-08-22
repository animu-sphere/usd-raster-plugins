# ADR-0006: Local-Origin Coordinate Policy

## Status

Accepted for Milestone 0. Implemented from Milestone 4.

## Decision

Source coordinates and USD stage coordinates are separate concepts. All
internal computation uses `double`; authored positions are `float` relative to
an explicit local origin authored as `geo:localOrigin`.

```text
stored pixel index
  -> pixel anchoring
  -> affine geotransform, in double
  -> source coordinate (CRS)
  -> subtract localOrigin
  -> stage-local float coordinate
```

Recovery is exact by construction:

```text
sourceCoordinate = geo:localOrigin + stageLocalPosition
```

One local origin is shared by an entire conversion, including every tile
payload. Per-tile origins are not used.

## Rationale

`float` has about 7 decimal digits of precision. A projected coordinate near
`500000.0` therefore resolves to roughly 3 cm, and one near `4000000.0` to
about 25 cm — visible wobble on any terrain surface, and worse than the
accuracy of the source data. Writing source coordinates directly into `points`
destroys precision that the file actually contained.

An explicit origin solves this without a custom numeric type, and it keeps the
transformation auditable: the origin is a value on the stage, so a consumer can
reconstruct the source coordinate without knowing anything about how the layer
was authored.

A shared origin across tiles is what makes tile boundaries exactly coincident.
With per-tile origins, two tiles compute the same boundary vertex through
different subtractions and can disagree in the last bits, producing cracks that
are difficult to diagnose and impossible to fix downstream.

`usd-pointcloud-plugins` adopted the same model in its own coordinate-model
ADR. Matching it means a consumer handling both data models has one recovery
formula, not two.

## Consequences

- The origin is part of the conversion manifest and of the cache key. It is
  computed once and reused; recomputing it per read would silently change
  authored geometry.
- The default origin is the centre of the raster extent, rounded to a stable
  value so that adding a tile or changing a window does not move it.
- Every prim carrying stage-local positions also carries `geo:localOrigin`,
  including each tile payload, so a tile loaded on its own is interpretable.
- The stage is Y-up with one meter per unit, so authoring applies a fixed axis
  mapping with a sign change on the north axis. That mapping lives in exactly
  one place and is pinned by a golden test.
- A geographic CRS in degrees has no metric interpretation and cannot use this
  policy directly. It is either rejected or converted through an explicit,
  recorded, lossy scale.

## Open questions

Resolved in later milestones, with real data:

- Whether PROJ is introduced for CRS interpretation, and if so, where the
  dependency boundary sits.
- Whether the vertical datum and epoch need a richer representation than
  preserved metadata.
- Whether an ECEF or ENU output option is worth adding for globe-scale
  composition.
