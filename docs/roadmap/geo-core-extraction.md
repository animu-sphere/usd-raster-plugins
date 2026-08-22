# Shared Geospatial Core Extraction

`usd-pointcloud-plugins` owns a `usdGeoCore` with bounds, transforms, CRS,
cache keys, diagnostics, and local origin. This repository starts with its own
`usdGeoCore`, modeled on that design and **not shared as code**.

This document records why, and what would have to be true before extracting a
third repository.

## Phase 1 — deliberate duplication

`usd-raster-plugins` implements its own `libs/usd-geo-core`. The design,
naming, and type shapes follow
`usd-pointcloud-plugins/libs/usd-geo-core` closely, so that a later merge is a
reconciliation rather than a rewrite. But the code is separate.

This is a deliberate cost. It is chosen because premature extraction is more
expensive than duplication:

- A shared library that is still being designed forces both consumers to move
  together for every change. Raster needs pixel anchoring and affine
  transforms; point clouds need neither. Adding them to a shared core before
  the raster side has settled means changing the point-cloud repository to
  serve a design that is still moving.
- Two implementations of the same concept, written independently against the
  same use cases, reveal which parts are genuinely common and which merely look
  similar. That information does not exist yet.
- A third repository adds versioning, release coordination, and cross-repo CI
  to both projects. That overhead is worth paying for a stable contract and not
  for a speculative one.

The concrete risk being avoided: a shared `Transform` type that has to grow a
`pixelAnchor` field because raster needed it, which then appears in point-cloud
APIs that can never use it.

## Phase 2 — the extraction gate

Extraction into `usd-geospatial-core` is considered only when **all** of the
following are true in both repositories, in shipped code:

| Concept | Agreement required |
| --- | --- |
| CRS representation | Same type, same optional fields, same EPSG/WKT precedence |
| Affine transform | Same coefficient order and same application semantics |
| Geospatial bounds | Same type and same inclusivity rules |
| EPSG / WKT metadata | Same preservation and reporting policy |
| Axis convention | Same source-to-stage mapping and the same sign handling |
| Local origin | Same computation, same recovery formula, same authored property |
| Diagnostics | Same value types, same severity semantics |
| Source identity | Same inputs and the same stability levels |
| Deterministic cache key | Same normalization and the same versioning inputs |

"Similar" is not "the same". If raster needs a field point clouds cannot use,
that concept has not converged, and the correct answer is that it stays in the
raster core.

Additionally:

- Both repositories must have shipped at least one release that exercises the
  concept in production, not only in tests.
- The shared surface must have been stable across at least two releases on both
  sides.

## Phase 3 — how extraction would work, if it happens

1. Reconcile the two implementations in place, so that the shared subset is
   byte-for-byte compatible in behavior and covered by the same test cases in
   both repositories.
2. Extract `usd-geospatial-core` with the reconciled subset only. Anything that
   did not converge stays where it is.
3. Both repositories depend on it as a pinned version, and neither vendors it.
4. Each repository keeps its own domain core — `usdRasterCore`,
   `usdPointCloudCore` — which never merge, because point attributes and raster
   windows are genuinely different data models.

## What is explicitly not a reason to extract

- "Both repositories have a file with the same name."
- "It would avoid duplicated code."
- "A third project might want it."

The condition is demonstrated convergence in shipped code, not resemblance and
not anticipation.

## Current status

Phase 1. `usd-raster-plugins` has not implemented `usdGeoCore` yet; when it
does, at Milestone 1, it does so independently.

Reassessment happens at each minor release of this repository, and the outcome
— including "no change" — is recorded here.
