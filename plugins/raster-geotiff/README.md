# raster-geotiff — OpenUSD GeoTIFF file-format plugin

The bundle registers `.tif` and `.tiff` and connects metadata and the initial
regular-grid mesh representation end to end.

## Layout

```
openstrata.plugin.yaml          bundle contract (identity, runtime range, provides, tests)
CMakeLists.txt                  builds the platform library into lib/
cmake/OpenStrataPlugin.cmake    pinned, self-contained build/install mechanics
src/RasterGeotiffFileFormat.{h,cpp}  the SdfFileFormat adapter
plugin/resources/raster-geotiff/plugInfo.json   USD plugin registration
tests/fixtures/                 basic (valid) + invalid (negative) fixtures
```

The copied CMake helper is versioned with this scaffold and requires neither an
OpenStrata checkout nor `ost` at build time. Keep bundle-specific targets,
components, resources, and tests in `CMakeLists.txt`; update helper mechanics by
reviewing a newer template rather than linking to the generator source tree.

## Workflow

```sh
ost plugin inspect raster-geotiff     # Level 0: bundle structure
ost plugin build raster-geotiff       # build the shared library into lib/
ost plugin doctor raster-geotiff      # staged diagnostics (L0–L1; L2+ need a real runtime)
ost plugin test raster-geotiff        # orchestrate the levels and write a report
```

The plugin parses no TIFF bytes itself. It adapts the resolver's `ArAsset` to
`RandomAccessSource`, drives `GeoTiffReader`, and calls the shared authoring
entry point.

## Supported file extensions

`.tif` and `.tiff` are both registered.

## Supported formats, compressions, and sample formats

The connected metadata path supports the GeoTIFF metadata currently exposed by
`usdGeoTiff`: classic TIFF and BigTIFF, uncompressed sources, and the sample
types listed by that reader. Pixel bytes are not decoded.

## FileFormat arguments

`representation=metadata|mesh` is supported, with `metadata` as the default.
`band` is a strict, 1-based positive integer and defaults to `1`. Mesh
representation also supports `heightScale`, `nodata=skip|fill|keep`, and the
required `fillValue` when `nodata=fill`. These mesh-only arguments are rejected
for the metadata representation.
`pixelAnchor=area|point` is accepted when the source omits
`GTRasterTypeGeoKey`. Unknown arguments and unsupported representation values
fail with `GTIF013`.

## Authored OpenUSD result

An open `.tif` or `.tiff` produces `/Raster` as a `Scope` for `metadata`, or as
a regular `UsdGeomMesh` for `mesh`, with the `geo:` and `raster:` metadata in
[`RASTER_METADATA.md`](../../docs/reference/RASTER_METADATA.md). The selected
band is used for metadata and mesh authoring. Mesh reads the selected band at
step 1 and applies the requested height scale and NoData policy. Sources without
georeferencing fail with `GTIF006`; a missing pixel anchor requires the
explicit argument.

## Cost and limits

The default representation is metadata and is bounded by the TIFF header, IFD,
and metadata value reads. Mesh reads are limited to 4,194,304 vertices. A
larger mesh request fails with `GTIF012`; use `usd-raster-convert` for a tiled
or lower-detail result.

## Plugin discovery and installation

`plugin/resources/raster-geotiff/plugInfo.json` is the discovery root. Set
`PXR_PLUGINPATH_NAME` to that directory after installing the bundle; see
[`INSTALL.md`](../../docs/guides/INSTALL.md).

The checked-in `plugInfo.json` is a source-tree inspection placeholder. The
CMake configure step regenerates its platform-specific `LibraryPath` before a
build or package is used.

## Build and test

```text
ost plugin build plugins/raster-geotiff
ost plugin test plugins/raster-geotiff --up-to 4
```

## Runtime dependencies

OpenUSD 26.08 and the in-repository `usdGeoTiff`, `usdRasterCore`,
`usdGeoCore`, and `usdRasterAuthoring` libraries are required. No resolver or
transport implementation is linked.

## Licensing

The plugin follows the repository Apache-2.0 license. No third-party codec is
linked for the metadata path.

## Known limitations

Sampling and LOD arguments, EPSG override, image representation, reprojection,
and resolver integration tests beyond the default local resolver are not yet
connected.

## Compatibility

The bundle targets OpenUSD `>=26.08,<27.0`, platform `cy2026`, profile `usd`.

## Co-hosting a Typed Schema

To compile a generated API schema into this same plugin, add `schema.usda` at the
bundle root and add `usd-schema:<TypeName>` to `provides` in
`openstrata.plugin.yaml`. `ost plugin build` runs `usdGenSchema` in the resolved
runtime environment, links the generated C++ API sources into this library, and
merges the generated `Types` into `plugin/resources/raster-geotiff/plugInfo.json`.
