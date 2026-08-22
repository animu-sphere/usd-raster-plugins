# Raster Format Support Order

GeoTIFF is the only committed format. This document records the order the
others would arrive in, and — more importantly — the gate each must pass, so
that a format is added because the infrastructure is ready rather than because
it was requested.

## Selection criteria

A candidate format is evaluated on:

1. **Contract fit.** Can it be expressed through `RasterGrid`, `RasterWindow`,
   and the existing georeferencing contract without widening them?
2. **Random-access value.** Does windowed reading actually help, or does the
   container force whole-file decoding?
3. **Georeferencing completeness.** Does the format carry CRS and transform
   in-band, or must the caller supply them?
4. **Dependency cost.** What does decoding it add to the dependency graph and
   to the redistribution obligations?
5. **Real demand.** Is there a workflow that needs it, rather than an
   expectation that someone might?

A format that fails criterion 1 is not added until the shared contract is
extended deliberately, with an ADR. Copying a format-specific assumption into
the shared layer to make one reader easier is what the layering exists to
prevent.

## Sequence

```text
GeoTIFF          committed, v0.1.0 - v0.7.0
  |
  v
COG              an optimization of the GeoTIFF reader, not a new format
  |
  v
PNG / JPEG + world file
  |
  v
NetCDF / GRIB
  |
  v
Zarr
```

### GeoTIFF — committed

Covered by [geotiff-vertical-slice.md](geotiff-vertical-slice.md) and
[tiling-and-lod.md](tiling-and-lod.md). Rationale in
[ADR-0004](../adr/0004-geotiff-first-format.md).

### COG — Milestone 9, not a separate reader

A Cloud-Optimized GeoTIFF is a GeoTIFF with internal tiling, overviews, and a
front-loaded layout. Treating it as a separate parser would duplicate the
entire TIFF path to gain nothing.

Entry gate: remote reads work (Milestone 8) and a measured baseline exists to
compare the optimization against.

### PNG / JPEG with a world file

Attractive because orthophoto deliverables often arrive this way, and because
the decoders are small.

Entry gate:

- The `image` representation exists and is proven on GeoTIFF RGB sources.
- The explicit-argument path for missing georeferencing is proven, because a
  world file is a sidecar the resolver must supply and the format does not
  carry a CRS at all.
- A sidecar-resolution contract exists: how a `.pgw` or `.jgw` is located
  through `ArResolver` without the plugin doing path arithmetic. This is the
  hard part, and it is a contract question rather than a decoding one.

Windowed reading is weak here: baseline PNG and JPEG do not support random
access to arbitrary regions cheaply. That is an honest limitation to state
rather than to engineer around.

### NetCDF / GRIB

The first genuinely multi-dimensional candidates, and the first that do not fit
the current contract without extension.

Entry gate:

- A variable-selection contract: a NetCDF file holds many variables across
  many dimensions, and "the raster" is a 2D slice of one of them. That
  selection is an argument surface this repository does not yet have.
- A time-and-level contract: which slice, and how it appears in USD. Possibly
  as time samples, possibly as separate prims — an open design question.
- A decision on the dependency. The reference libraries are substantial, and
  [ADR-0007](../adr/0007-gdal-not-a-core-dependency.md) applies to them by the
  same reasoning it applies to GDAL.

### Zarr

Chunked arrays over object storage. Architecturally the best fit for the
windowed reader, and the worst fit for the current asset model.

Entry gate:

- A Zarr store is a directory of many objects, not a single asset. Reading it
  through `ArResolver` requires a directory-shaped asset contract that OpenUSD
  does not naturally provide.
- Resolver maturity: many small reads amplify latency, so the block-cache
  behavior of `usd-http-resolver` matters more here than anywhere else.
- The same variable-selection contract NetCDF needs.

## Delivery stages per format

Every format follows the same staged path, so that partial support is honest
rather than surprising:

| Stage | Deliverable | Status word |
| --- | --- | --- |
| 1 | Container inspection and metadata | `metadata only` |
| 2 | Windowed pixel reads for the common encodings | `implemented, not connected` until an argument reaches it |
| 3 | Mesh or image authoring through the shared entry point | `implemented` |
| 4 | Tiled and payload-backed authoring | `implemented` |
| 5 | Resolver-backed and remote validation | `implemented` |

A format never appears in the capability matrix as "supported" without naming
its stage.

## Deferred and not planned

| Item | Status | Reason |
| --- | --- | --- |
| Raster writing | deferred | Read behavior and preservation rules first |
| Reprojection | not planned | Belongs to a projection library or the caller |
| Vertical datum transformation | not planned | Same |
| Vector formats | not planned | A different data model; a different repository |
| Point clouds | not planned | `usd-pointcloud-plugins` owns them |
| Mosaicking and virtual rasters | not planned yet | Composition of many sources; possibly a USD composition problem rather than a reader problem |
