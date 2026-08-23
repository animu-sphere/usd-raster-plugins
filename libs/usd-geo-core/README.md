# usdGeoCore

## Purpose

Format-independent geospatial values and the typed diagnostic vocabulary that
every other module in this repository shares. It is the bottom of the
dependency graph: it depends on nothing, so that a CRS value, a spatial
extent, a tile identity, or a diagnostic means the same thing in a reader, in
the tiling module, in the authoring library, and in a plugin.

**OpenUSD is not required.** This module builds and tests with no OpenUSD
runtime present, and that property is verified per pull request by the core
lane and by `tools/check_core_headers.py`.

## Responsibilities

- `Vec3d` — a point or offset in source coordinates, always `double`.
- `GeoBounds` — an axis-aligned extent in source coordinates.
- `CrsDescription` — EPSG code, WKT, citation, linear and angular units, and
  vertical CRS, all optional and all preserved as read.
- `LocalOrigin` — the stable offset that makes `float` stage positions viable,
  and the exact conversion in both directions.
- `TileId` — deterministic tile identity anchored at source pixel `(0, 0)`,
  with its stable string spelling.
- `CacheKey` — normalized cache-key accumulation with unambiguous field
  framing and a host-independent digest.
- `Diagnostic`, `DiagnosticCode`, `Severity`, `PixelWindow`, `DiagnosticSink` —
  the shared diagnostic vocabulary.

## Non-responsibilities

`usdGeoCore` does not parse any file format, does not perform I/O, does not
reproject, does not transform vertical datums, does not compute geodetic
distances or areas, and does not know that OpenUSD exists. It does not link
PROJ, GDAL, libtiff, or any transport library, and it never will: section 2 of
[WORKSPACE.md](../../docs/architecture/WORKSPACE.md) lists its allowed
dependencies as none.

It also does not apply the up-axis mapping. `LocalOrigin` subtracts the origin
and stops there; remapping X/Y/Z onto a Y-up stage happens in exactly one
place, `usdRasterAuthoring`, so the sign convention has one home.

## Public API

| Header | Provides |
| --- | --- |
| `usdgeo/Vec3d.h` | `Vec3d` and its arithmetic |
| `usdgeo/GeoBounds.h` | `GeoBounds`, `Expand`, `Union`, `Center`, `Size` |
| `usdgeo/CrsDescription.h` | `CrsDescription`, `CrsKind`, `LinearUnit`, `AngularUnit`, `GetMetresPerUnit` |
| `usdgeo/LocalOrigin.h` | `LocalOrigin`, `FromBounds`, `ToStageLocal`, `ToSource` |
| `usdgeo/TileId.h` | `TileId`, `ToString`, `Hash`, ordering, `std::hash` |
| `usdgeo/CacheKey.h` | `CacheKey` and its typed `Add*` methods |
| `usdgeo/Diagnostic.h` | `Diagnostic`, `DiagnosticCode`, `Severity`, `PixelWindow`, `DiagnosticSink` |

### Minimal usage

```cpp
#include <usdgeo/GeoBounds.h>
#include <usdgeo/LocalOrigin.h>

auto bounds = usdgeo::GeoBounds::Empty();
bounds.Expand(usdgeo::Vec3d{300000.0, 4400000.0, 10.0});
bounds.Expand(usdgeo::Vec3d{300100.0, 4400100.0, 40.0});

const auto origin = usdgeo::LocalOrigin::FromBounds(bounds);
const auto local = origin.ToStageLocal(usdgeo::Vec3d{300012.5, 4400087.25, 33.5});
// local is small enough to narrow to float; origin.ToSource recovers the
// source coordinate exactly.
```

## Dependencies

None beyond the C++17 standard library. This is enforced, not assumed:
`tools/check_core_headers.py` runs as the `core_dependency_check` CTest case
and fails on any include outside `usdgeo/` and the standard library.

## Data flow

```text
reader / tiling / authoring / plugin
            |
            v  values and diagnostics
        usdGeoCore
```

Everything depends on this module; it depends on nothing. `usdRasterCore` adds
the raster-specific model on top.

## Error and diagnostic behavior

This module defines the diagnostic vocabulary rather than emitting diagnostics.
Its own functions do not fail: they are total on their inputs, and the
degenerate cases have defined results rather than error paths.

- `GeoBounds::Expand` ignores non-finite components. A single NaN elevation
  must not poison an extent; a sentinel elevation is NoData's problem.
- `GeoBounds::Size` on an invalid extent is zero, not negative, and
  `GeoBounds::Center` is the origin rather than NaN. `Empty()` is the
  documented starting point for an accumulator, so both accessors have to be
  total on it — a NaN centre would propagate into a local origin and out to
  every authored position.
- `LocalOrigin::FromBounds` on an invalid extent is the origin at zero.
- `GetDiagnosticCodeName` returns a stable identifier, never localized text.

`DiagnosticSink` accumulates so that one pass over a malformed source reports
every problem rather than only the first. `HasError` reflects rule 3 of
[DIAGNOSTICS.md](../../docs/architecture/DIAGNOSTICS.md): a warning must leave
a result the caller can still use.

## Threading and ownership

Every type here is a value type with no shared state, no allocation beyond its
own members, and no internal synchronization. Copies are independent. Distinct
instances may be used from distinct threads without coordination; a single
instance mutated concurrently is a data race, as for any value.

No API returns a borrowed buffer or a pointer into internal storage, so there
are no lifetime rules for a caller to observe.

## Coordinate spaces

Every value in this module is in **source coordinates** — the coordinates of
the source CRS, in `double` — except where a name says otherwise:

| Type | Space |
| --- | --- |
| `Vec3d`, `GeoBounds` | source coordinates |
| `LocalOrigin::GetValue` | source coordinates |
| `LocalOrigin::ToStageLocal` result | stage-local offset, still `double` |
| `PixelWindow` | source **pixel** coordinates, full resolution |
| `TileId` | tile indices, anchored at source pixel `(0, 0)` |

Nothing here is in stage coordinates after the up-axis mapping; that space
exists only inside `usdRasterAuthoring`. See
[COORDINATE_MODEL.md](../../docs/architecture/COORDINATE_MODEL.md).

## Build and test

Part of the root CMake build, and of the OpenUSD-free core lane:

```text
cmake -S . -B build-core -DUSDRASTER_CORE_ONLY=ON
cmake --build build-core
ctest --test-dir build-core -R usdGeoCore
```

Tests are a plain executable with no framework dependency, because the core
lane must build on a machine with no package manager and no OpenUSD.

## Known limitations

- `CacheKey` is not a cryptographic hash. It defends against accidental
  collision between distinct requests, not against a constructed one. If a
  generated cache ever spans a trust boundary, that is when it is replaced, and
  the replacement gets its own ADR.
- `LinearUnit` covers metre, international foot, and US survey foot. A source
  with any other unit keeps its conversion factor in `linearUnitMetres` and
  reports `Unknown` for the enumerator, so nothing is lost, but nothing
  classifies it either.
- `LocalOrigin::FromBounds` quantizes to a caller-supplied quantum, defaulting
  to one metre. Whether that default is right for a geographic CRS in degrees
  is an open question owned by milestone 4.

## Planned work

| Item | Status | Milestone |
| --- | --- | --- |
| Value types, diagnostics, tile identity, cache keys | implemented | 1 |
| Source identity for resolver-backed sources | planned | 8 |
| Normalized cache-key inputs for the generated cache | planned | 7 |

The task-level record is
[implementation-status.md](../../docs/roadmap/implementation-status.md).
