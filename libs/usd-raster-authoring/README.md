# usdRasterAuthoring

## Purpose

Shared OpenUSD authoring for raster representations. The connected entry
points author metadata and an initial regular-grid mesh at `/Raster`.

## Responsibilities

`AuthorMetadata` maps validated `RasterMetadata` values to the property names
and types in [`RASTER_METADATA.md`](../../docs/reference/RASTER_METADATA.md).
`AuthorMesh` converts a decoded `RasterGrid` into `UsdGeomMesh` points,
topology, extent, local-origin coordinates, and a conversion record.

## Non-responsibilities

This module does not parse TIFF, read pixels, resolve assets, parse plugin
arguments, choose a CRS, or perform reprojection. Image authoring and tiled
conversion remain later capabilities.

## Public API

`AuthorMetadata` accepts project-owned raster values and a `SdfLayer`; it does
not expose TIFF or transport types. `AuthorMesh` additionally accepts an
owned, decoded `RasterGrid` and authors a regular-grid mesh.

## Dependencies

The module depends on `usdGeoCore`, `usdRasterCore`, and OpenUSD `sdf`, `tf`,
and `vt`. It is the only library in this repository that includes OpenUSD
headers.

## Data flow

`GeoTiffReader -> RasterMetadata + RasterGrid -> AuthorMesh -> SdfLayer /Raster`.
Source coordinates are computed in double precision and narrowed only for the
stage-local mesh points after subtracting the authored local origin.

## Error and diagnostic behavior

Invalid layer, missing georeferencing, missing bands, or failed attribute
creation produce typed diagnostics. No sentinel metadata is authored for an
absent source value.

## Threading and ownership

The caller owns the layer and metadata. The function performs synchronous
authoring and owns no returned buffers or shared mutable state.

## Build and test

The module builds with the OpenUSD runtime through `ost plugin build
plugins/raster-geotiff`. Core metadata parsing remains testable without USD.

## Known limitations

Only the initial regular-grid mesh path is connected. The plugin uses sampling
step 1; height scale and the `skip`, `fill`, and `keep` NoData policies are
available, while LOD profiles, CRS override arguments, and tile payloads are
not implemented.

## Planned work

Extend the shared mesh path with the remaining coordinate goldens, dynamic
arguments, and bounded tiled conversion. See
[`REPRESENTATIONS.md`](../../docs/architecture/REPRESENTATIONS.md).