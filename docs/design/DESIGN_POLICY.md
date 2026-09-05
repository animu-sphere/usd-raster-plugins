# Development Policy

Last updated: 2026-09-04

This document is the standing development policy for `usd-raster-plugins`. The
roadmap, format support order, and architecture documents refine it; they do
not override it.

## 1. Purpose

`usd-raster-plugins` provides format-independent raster ingestion, windowed
reading, sampling, tiling, metadata, cache, and OpenUSD authoring
infrastructure that brings raster and grid geospatial data — elevation models,
orthophotos, and classified rasters — into OpenUSD workflows.

GeoTIFF establishes the current implementation. Future formats such as COG
optimizations, world-file-backed PNG/JPEG, NetCDF/GRIB, and Zarr must adapt to
the shared `RasterGrid` and reader contracts rather than introducing parallel
paths. The shared contracts do not depend on a single format, transport, or
OpenUSD version. The order and entry gates are in
[format support order](../roadmap/format-support-order.md).

Point-cloud formats belong to `usd-pointcloud-plugins`. HTTP, authentication,
Range requests, retry policy, and raw-byte caching belong to
`usd-http-resolver`. Renderer-specific material graphs, raster writing, and
general-purpose GIS processing are outside the active scope of this
repository.

Both direct FileFormat access and explicit conversion are supported. Direct
access serves inspection and preview; `usd-raster-convert` is the production
path for long-running deterministic payload and cache generation.

## 2. Current Assessment

Milestones 0 through 3 are complete for the v0.2.0 scope. The GeoTIFF
metadata, window-reading, and initial regular-grid mesh slices are connected
and tested; v0.2.0 records the completed pixel-read release. The task-level record is
[implementation status](../roadmap/implementation-status.md).

The properties to establish and then preserve are:

- Raster parsing is separated from OpenUSD API usage.
- The core libraries and readers build and test without OpenUSD.
- The reader is windowed and random-access, never whole-image.
- Geospatial metadata, raster contracts, USD authoring, and plugin
  responsibilities have clear boundaries.
- Readers remain independent of OpenUSD and transport policy.
- Coordinate interpretation — pixel anchoring, row direction, affine
  transform, unit, and local origin — is written down and covered by golden
  tests before it is optimized.

The standing goal is not a larger format count. It is that any raster format
can use the same windowed reading, sampling, tiling, caching, and OpenUSD
authoring infrastructure:

```text
format-independent windowed reading, sampling, tiling, caching, and OpenUSD authoring
```

## 3. Design Principles

### 3.1 Dependency Direction

```text
raster source -> format reader -> RasterGrid / RasterWindow
                                        |
                                        +-> usdRasterTiling
                                        +-> usdRasterAuthoring -> OpenUSD

ArResolver -> ArAsset adapter -> RandomAccessSource -> usdGeoTiff
```

Rules:

- Core libraries do not depend on the OpenUSD API.
- Format readers never author USD prims.
- OpenUSD-specific representation stays inside `usdRasterAuthoring`.
- FileFormat Plugins remain thin adapters. A plugin normalizes arguments,
  opens a source, invokes a reader, calls one shared authoring entry point,
  and converts diagnostics. Decode loops, window planning, and metadata
  interpretation do not live in a plugin.
- No plugin implements transport. See
  [ADR-0002](../adr/0002-resolver-owns-transport.md).

### 3.2 Format-Independent Contracts

The following belong in the shared layer, not in a format-specific layer:

- CRS and coordinate reference metadata
- Linear and vertical units
- Affine geotransform and pixel anchoring
- Source origin and local origin
- Spatial bounds
- Raster size, band descriptions, and sample types
- NoData representation
- Window, tile, and overview identity
- Cache key inputs
- Diagnostic codes

### 3.3 Existing Schemas Before Custom Schemas

No repository-specific USD schema is defined. A raster is authored through
existing OpenUSD schemas — `UsdGeomMesh` for elevation grids, image assets
with `UsdPreviewSurface` / `UsdUVTexture` for orthophotos, and namespaced
metadata for everything a standard schema does not carry. "Heightfield" is an
internal raster semantic, not a schema. See
[ADR-0001](../adr/0001-existing-usd-schemas.md) and
[REPRESENTATIONS.md](../architecture/REPRESENTATIONS.md).

### 3.4 One Source, Several Representations

A raster source is not fixed to one prim shape. The same GeoTIFF may be
authored as metadata only, as an elevation mesh, or as an image plane,
selected by a file-format argument. The representation is part of layer
identity and of the cache key.

Ingestion and representation remain separate. A format backend produces the
shared metadata and windowed raster model; authoring and conversion consume
that model to produce images, heightmaps, meshes, masks, or analytical
attributes. Adding a representation must not require adding format-specific
logic to the authoring layer.

### 3.5 Windowed Reading

`ReadAllPixels()` is not the central API. The reader exposes windows, tiles,
and scanlines so that bounded memory, remote sources, COG overviews, and
partial reads all remain reachable. See
[RASTER_READER.md](../architecture/RASTER_READER.md) and
[ADR-0003](../adr/0003-windowed-raster-reader.md).

### 3.6 File-Format Arguments and Layer Identity

An argument changes what a layer contains, so it changes layer identity.
Normalized arguments participate in layer identity and cache keys, and an
argument that alters authored topology never reuses a layer cached under a
different value. Arguments are validated, never guessed, and never silently
ignored. Argument parsing lives in the plugin; validation lives in the shared
layer. The full contract is in
[FILE_FORMAT_ARGUMENTS.md](../architecture/FILE_FORMAT_ARGUMENTS.md).

Expose profiles before knobs. `lod=preview|balanced|quality` is preferable to
publishing a raw sampling step as the primary interface, because a profile can
be retuned while the argument surface stays stable.

### 3.7 Data Preservation

Input meaning is not lost.

- Retain band metadata, sample format, and unit information that can be read.
- Convert to standard USD attributes when a natural mapping exists.
- Keep metadata without a standard representation as namespaced metadata or
  primvars.
- Make lossy conversions — resampling, quantization, NoData substitution —
  explicit and recorded in authored metadata.
- Keep the relationship between source coordinates and stage-local
  coordinates recoverable through `geo:localOrigin`.

### 3.8 Precision

Large projected or geographic coordinates are never assigned directly into
`float`. Internal computation uses `double`; authored positions are `float`
relative to an explicit local origin.

```text
world = localOrigin + stageLocalPosition
```

See [COORDINATE_MODEL.md](../architecture/COORDINATE_MODEL.md) and
[ADR-0006](../adr/0006-local-origin-coordinate-policy.md).

### 3.9 Bounded Memory

On-disk size is not a bound on process memory. Every authoring path that can
encounter a large source must be expressible as a sequence of bounded windows.
Full-mesh authoring of an arbitrarily large raster is a preview convenience
with a documented ceiling, not the production path. The production path is
tiling plus payloads, driven by the converter.

### 3.10 Preview Path and Production Path

```text
FileFormat Plugin  ->  preview / interactive / inspection
usd-raster-convert ->  long-running / deterministic / production
```

Long conversions are not pushed into `SdfFileFormat::Read`. See
[ADR-0008](../adr/0008-preview-vs-converter.md).

## 4. Format Coverage

| Format | Status | Notes |
| --- | --- | --- |
| GeoTIFF (strip and tile, BigTIFF) | first target | metadata, then windows, then mesh |
| COG | planned as an optimization | overview discovery and tile-aware reads inside the GeoTIFF reader, not a separate parser |
| PNG / JPEG + world file | candidate | no in-band georeferencing; requires explicit arguments |
| NetCDF / GRIB | candidate | multi-dimensional; needs a variable-selection contract first |
| Zarr | candidate | chunked object storage; depends on resolver maturity |

Raster writing is deferred until read behavior and preservation rules are
stable.

## 5. Dependency Policy

- GDAL is the preferred general raster backend. It is introduced behind the
  project-owned raster reader abstraction so GDAL types, driver selection, and
  virtual-filesystem policy do not leak into plugins or shared authoring code.
  GeoTransform, CRS, band metadata, NoData, overviews, reprojection, and
  windowed reads are delegated to GDAL where that backend is selected. See
  [ADR-0009](../adr/0009-gdal-raster-backend.md).
- The implemented libtiff-backed `usdGeoTiff` path remains a supported,
  lightweight specialized backend. It stays isolated behind the same shared
  contracts and continues to use client I/O over `RandomAccessSource`.
- GDAL `/vsicurl/` is not used by plugin code. Remote transport remains owned
  by `ArResolver`; the GDAL backend must use a project-owned adapter over
  `RandomAccessSource` before it can participate in resolver-backed reads.
- No transport library, HTTP client, cloud SDK, or resolver implementation is
  a build-time dependency of this workspace.
- Large dependencies stay optional and scoped to the owning target.

## 6. Testing Policy

Four test layers, in order of cost:

1. **Core tests** — no OpenUSD. TIFF/IFD parsing, GeoTIFF keys, affine
   transforms, window arithmetic, band decoding, NoData, sampling, tiling.
2. **Golden tests** — many small synthetic rasters with recorded expected
   output, covering odd sizes, tiled and stripped layouts, `uint8` / `uint16` /
   `float32`, NoData, rotated transforms, negative scale, and both pixel
   anchoring conventions.
3. **Backend equivalence tests** — comparison between the specialized
  GeoTIFF reader and GDAL where both support the same source. GDAL may also be
  used as an oracle for generated fixtures.
4. **Plugin and resolver tests** — GeoTIFF through `SdfFileFormat` to
   `SdfLayer` and `UsdStage`, and the same fixture read from a local file, an
   in-memory `ArAsset`, and a resolver-provided `ArAsset` with identical
   results.

Fixtures are generated by a checked-in script wherever possible, so a fixture
is reviewable as code rather than as an opaque binary.

## 7. Diagnostics Policy

Diagnostics are typed values, not bare strings. Every diagnostic carries a
stable code, a severity, a message, and — where meaningful — a byte offset,
pixel coordinate, or window identity. Unsupported input fails with a specific
code; it never degrades silently into an empty or partially authored stage.
See [DIAGNOSTICS.md](../architecture/DIAGNOSTICS.md).

## 8. Documentation Policy

Every module under `libs/` and `plugins/` carries a `README.md` that states
what it owns and, explicitly, what it refuses to own. A contract change
without a README change is an incomplete change. See
[MODULE_README_CONTRACT.md](../contributing/MODULE_README_CONTRACT.md).

## 9. Shared Geospatial Core

`usd-pointcloud-plugins` already owns a `usdGeoCore` with bounds, transforms,
CRS, cache keys, diagnostics, and local origin. This repository begins with its
own raster-oriented `usdGeoCore`, deliberately modeled on that design but not
shared as code.

Extraction into a third repository is considered only after the two
implementations actually agree on CRS representation, affine transform,
geospatial bounds, EPSG/WKT metadata, axis convention, local origin,
diagnostics, source identity, and deterministic cache key. "It looks shareable"
is not an extraction condition. See
[geo-core extraction](../roadmap/geo-core-extraction.md).

## 10. Anti-Goals

Recorded because each one is an attractive shortcut that removes a capability
later:

1. A custom raster USD schema before existing schemas are proven insufficient.
2. A parser written directly inside the FileFormat Plugin.
3. Letting a backend, including GDAL, bypass the project reader contracts or
  the resolver-owned transport boundary.
4. `ReadAllPixels()` as the central reader API.
5. HTTP implemented inside a plugin.
6. One `UsdGeomMesh` for an arbitrarily large raster as the only path.
7. Point-cloud and raster data models merged into one repository.
