# ADR-0009: GDAL Is the Preferred General Raster Backend

## Status

Accepted on 2026-09-04. Supersedes ADR-0007 and the libtiff-only backend
portion of ADR-0004. ADR-0004's GeoTIFF-first decision remains accepted.

## Decision

Add `usdRasterGdal` as a production raster backend behind the
format-independent contracts in `usdRasterCore`. GDAL is the preferred backend
for general raster dataset access and future format expansion. It supplies
dataset discovery, CRS and geotransform metadata, bands, NoData, overviews,
window reads, and optional reprojection without exposing GDAL types to plugin,
tiling, authoring, or converter APIs.

The existing `usdGeoTiff` reader remains supported as a lightweight,
specialized GeoTIFF backend. Its libtiff integration and tested decoding work
are not discarded. Fixtures supported by both backends must produce equivalent
shared metadata and raster windows.

```text
                       +-> usdGeoTiff   -> libtiff
RandomAccessSource ----|
                       +-> usdRasterGdal -> GDAL
                                |
                                v
                  RasterMetadata / RasterGrid
                                |
                  authoring / conversion layers
```

GDAL does not own transport in this architecture. Plugin code must not use
`/vsicurl/`, GDAL cloud drivers, or a second credential and cache path. A
project-owned GDAL VSI adapter over `RandomAccessSource` is required before the
backend can claim resolver-backed or remote support.

## Rationale

GeoTIFF exercises the initial contracts well, but implementing each raster
container, CRS dialect, overview model, and reprojection path separately would
delay format expansion and duplicate mature ecosystem behavior. GDAL provides
a common dataset abstraction for GeoTIFF, COG, NetCDF, HDF5-derived rasters,
JPEG 2000, and later formats while preserving a project-owned API boundary.

Keeping the specialized GeoTIFF path has independent value: it offers a
smaller deployment option, validates the abstraction against a second backend,
and preserves direct control over TIFF range reads. The two backends are
complementary rather than layered on top of one another.

## Consequences

- `usdRasterGdal` is a production module, but remains optional at build and
  packaging time because host environments differ in dependency constraints.
- GDAL driver registration is explicit and limited to drivers the package
  claims; ambient driver availability does not silently expand support.
- The capability matrix reports support per backend and distinguishes local
  from resolver-backed access.
- Backend equivalence tests compare metadata, transforms, NoData, and decoded
  windows for shared fixtures.
- Reprojection is exposed only through explicit conversion APIs. A FileFormat
  Plugin does not silently reproject a source while opening it.
- libtiff remains private to `usdGeoTiff`; GDAL remains private to
  `usdRasterGdal`.

## Delivery Gate

The first production slice must open a local GeoTIFF through GDAL, map its
metadata into `RasterMetadata`, read a bounded single-band window into
`RasterGrid`, and pass equivalence tests against `usdGeoTiff`. Resolver-backed
access follows only after the VSI adapter passes the existing source
selectivity and transport-equivalence tests.