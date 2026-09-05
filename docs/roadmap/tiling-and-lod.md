# Tiling, Conversion, and Remote Access

Milestones 6 through 9 in detail. Where
[geotiff-vertical-slice.md](geotiff-vertical-slice.md) makes one raster work,
this sequence makes large and remote rasters work.

The contracts these milestones implement are
[TILING.md](../architecture/TILING.md),
[CACHE.md](../architecture/CACHE.md), and
[RESOLVER_SOURCE.md](../architecture/RESOLVER_SOURCE.md).

---

## Milestone 6 — Raster tiling (`v0.4.0`)

### The problem

A 40000 x 40000 float32 DEM is 1.6 billion vertices at full density. There is
no useful single `UsdGeomMesh` there: the stage, the memory, and Hydra all
concentrate the cost in one prim that cannot be partially loaded.

### Scope

`libs/usd-raster-tiling` and payload authoring in `usdRasterAuthoring`.

- The four tile concepts kept distinct: `SourceTile`, `RasterWindow`,
  `SpatialTile`, `PayloadTile`.
- Deterministic tile identity anchored at source pixel `(0, 0)`.
- Spatial tile planning: source extent to a tile grid at each level, with
  truncated margin tiles rather than padded ones.
- Shared-edge tiles: `tileSize + 1` vertices per axis, with boundary vertices
  exactly coincident because they come from the same source pixels through the
  same transform and the same shared local origin.
- Level construction, stopping when a level fits in one tile.
- Payload-backed authoring: one `.usdc` per tile, plus a root layer.
- The tile manifest.
- One-tile-at-a-time processing, so peak memory depends on `tileSize` rather
  than on source size.

### Exit criteria

- Peak memory during a tiled conversion is independent of source size,
  measured across sources spanning two orders of magnitude.
- Adjacent tiles have bit-identical shared boundary vertices — asserted, not
  eyeballed.
- Margin tiles on a source whose size does not divide evenly by `tileSize`
  cover the remaining extent with no gap and no overlap.
- Two conversions of the same source with the same arguments produce
  byte-identical payload files and names.
- The root layer composes, and loading one tile payload in isolation yields a
  prim interpretable from its own metadata.

### Explicitly not in this milestone

View-dependent selection, quadtree refinement driven by terrain error, and any
Hydra or camera awareness. This milestone authors a hierarchy; consuming it is
the responsibility of the host.

---

## Milestone 7 — Converter (`v0.5.0`)

### Scope

`tools/usd-raster-convert`.

```bash
usd-raster-convert \
  terrain.tif \
  Terrain.usda \
  --representation mesh \
  --lod balanced \
  --tile-size 512
```

- Deterministic, byte-stable output.
- Manifest generation, including I/O counters, peak memory, and per-tile
  timings.
- Generated-cache population with atomic publication.
- Resumability: an interrupted conversion restarts without redoing completed
  tiles.
- Progress reporting and meaningful cancellation, both of which a FileFormat
  read cannot offer.

### Exit criteria

- Converter output at `lod=off` on a small fixture is identical to a plugin
  read of the same fixture at `lod=off`. A difference between the preview path
  and the production path is a bug, and this test is what catches it.
- A conversion interrupted at any tile boundary resumes to the same final
  output.
- A published cache entry is never observable half-written.
- Rerunning an unchanged conversion is a cache hit with no re-authoring.

### Why the converter comes after tiling

The converter orchestrates tiling. Building it first would put tiling policy in
a CLI, where the plugin cannot reach it. See
[ADR-0008](../adr/0008-preview-vs-converter.md).

---

## Milestone 8 — Resolver interoperability (`v0.6.0`)

### Scope

The `ArAsset` adapter in the plugin bundle, and the equivalence guarantees
around it.

- `ArAssetSource : public RandomAccessSource`, with no scheme inspection, no
  retries, and no caching.
- Local reads moved onto the same interface, so remote is not a parallel path.
- Source identity derived from resolver-provided values, with conservative
  cache reuse.
- An in-repository test `ArResolver` fixture serving a synthetic scheme from
  memory, excluded from product discovery and release matrices.

### Exit criteria

- The same fixture read from a local file, a memory source, and a
  resolver-provided asset produces identical authored stages.
- A metadata-only read of a remote fixture touches kilobytes, proven by the
  instrumented source; a regression that turns it into a whole-file read fails
  a test rather than a benchmark.
- Short reads and unavailable assets map to the documented diagnostics.
- Cache reuse is disabled, with a warning, when identity inputs are
  insufficient.
- The required CI gate passes with no external resolver repository present.

### Tier 2

Integration against a real `usd-http-resolver` build runs outside the required
gate, validating end-to-end remote GeoTIFF reads and byte-range behavior
against a real server. It is composed separately precisely so that this
repository never acquires a build-time dependency on a transport
implementation. See [ADR-0002](../adr/0002-resolver-owns-transport.md).

---

## Milestone 9 — COG optimization (`v0.7.0`)

### Scope

COG is not a new parser. It is a layout convention over GeoTIFF, so this
milestone is an optimization of the existing reader.

- Harden the existing overview selection against remote sources and record
  the selected level in remote performance reports.
- Tile-aware read planning that prefers native tile boundaries.
- Reporting which overview level served each request.
- Remote selectivity metrics: requested bytes, fetched bytes, request count,
  amplification ratio.
- A performance baseline on real data, recorded so later changes are
  measurable.

### Exit criteria

- A preview of a large COG uses the selected overview through the resolver
  source, proven by the level recorded in metadata and by the byte counters.
- Bytes fetched for a preview drop by an order of magnitude against the
  Milestone 8 baseline on the same source.
- A GeoTIFF without overviews still works, decimating full-resolution data with
  no behavior change.
- The baseline is reproducible and recorded in a release report.

### Why last

Overview selection only pays off once remote reads work, and its value can only
be stated against a baseline. Doing it earlier would mean optimizing without a
measurement to compare against.
