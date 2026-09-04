# ADR-0004: GeoTIFF Is the First Format

## Status

Accepted for the GeoTIFF-first decision. The libtiff-only backend portion was
superseded by [ADR-0009](0009-gdal-raster-backend.md) on 2026-09-04.

## Decision

GeoTIFF is the only format targeted through v0.7.0. No second format is added
until the GeoTIFF contract is stable across metadata, windowed reads, mesh
authoring, tiling, conversion, and resolver-backed access.

libtiff is the intended TIFF decoding backend, isolated behind `usdGeoTiff` and
driven through a client-I/O adapter so that decoding sits on top of
`RandomAccessSource`:

```text
libtiff (TIFFClientOpen custom I/O)
   -> RandomAccessSource
   -> local file / memory fixture / ArAsset
```

GeoTIFF key decoding uses libgeotiff where it is available in the pinned
runtime, and a minimal in-repository decoder otherwise. That choice is deferred
to the milestone that implements it, and is recorded there.

## Rationale

GeoTIFF is chosen over the alternatives because it exercises every contract
this repository needs, at once:

- It is the representative raster GIS format; a plugin that cannot read GeoTIFF
  is not useful regardless of what else it reads.
- One format covers DEM, DSM, orthophoto, and classified raster, so the
  representation contract is validated against several real uses.
- It carries CRS, geotransform, NoData, and band metadata together, so the
  georeferencing contract is exercised rather than postponed.
- Tiled TIFF and BigTIFF make random access genuinely valuable, which
  validates [ADR-0003](0003-windowed-raster-reader.md) rather than leaving it
  theoretical.
- It composes naturally with HTTP Range requests, which validates
  [ADR-0002](0002-resolver-owns-transport.md) against a real remote workflow.
- COG is not a different format. It is a layout convention over GeoTIFF, so the
  remote optimization path is an extension of the same reader rather than a
  second parser.

libtiff is chosen over a from-scratch TIFF parser because TIFF has a long tail
of compression schemes, predictors, and layout variants that are expensive to
reimplement and easy to get subtly wrong, and because it supports custom I/O,
which is the one property a from-scratch parser would otherwise be needed for.

## Consequences

- `usdGeoTiff` owns the libtiff integration and never leaks libtiff types
  through its public API. The plugin does not include libtiff headers.
- The custom I/O adapter is a required part of the integration, not an
  optimization. Using libtiff file-based open would break remote support.
- libtiff licensing obligations are recorded in
  [DISTRIBUTION.md](../guides/DISTRIBUTION.md), and the pinned version is
  verified at release.
- If the pinned runtime cannot supply a usable libtiff, the fallback is a
  minimal in-repository decoder covering the sample formats and compressions in
  the capability matrix — a larger cost, and a known risk.
- The format sequence after GeoTIFF is recorded in
  [format support order](../roadmap/format-support-order.md), and a new format
  must adapt to the shared contracts rather than extending them ad hoc.
