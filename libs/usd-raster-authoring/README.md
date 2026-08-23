# usdRasterAuthoring

## Purpose

Shared OpenUSD authoring for raster representations. The first connected entry
point authors the metadata-only `/Raster` `Scope`.

## Responsibilities

`AuthorMetadata` maps validated `RasterMetadata` values to the property names
and types in [`RASTER_METADATA.md`](../../docs/reference/RASTER_METADATA.md).

## Non-responsibilities

This module does not parse TIFF, read pixels, resolve assets, parse plugin
arguments, choose a CRS, or perform reprojection. Mesh and image authoring are
later capabilities.

## Public API

`AuthorMetadata` accepts project-owned raster values and a `SdfLayer`; it does
not expose TIFF or transport types. The metadata representation authors a
`Scope`, never a mesh or pixel buffer.

## Dependencies

The module depends on `usdGeoCore`, `usdRasterCore`, and OpenUSD `sdf`, `tf`,
and `vt`. It is the only library in this repository that includes OpenUSD
headers.

## Data flow

`GeoTiffReader -> RasterMetadata -> AuthorMetadata -> SdfLayer /Raster`.
Source coordinates remain doubles in metadata; no stage-local point buffer is
created by this entry point.

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

Only metadata authoring is connected. Mesh, image, conversion records, and
tile payloads are not implemented.

## Planned work

Add the shared mesh authoring path after windowed pixel reads and coordinate
goldens land. See [`REPRESENTATIONS.md`](../../docs/architecture/REPRESENTATIONS.md).