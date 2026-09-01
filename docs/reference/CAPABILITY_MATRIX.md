# Capability Matrix

What raster input is accepted, and what it becomes in USD.

The GeoTIFF metadata reader and the window reader are connected to the
`raster-geotiff` FileFormat plugin's library path. Deflate, LZW, and PackBits
decoding are available when the optional libtiff backend is enabled; floating-
point predictor support and read planning remain later M3 work. Rows below distinguish the library
capability from the user-facing bundle.

Status vocabulary, from
[MODULE_README_CONTRACT.md](../contributing/MODULE_README_CONTRACT.md):

```text
implemented                   in this module, tested
implemented, not connected    exists in a library, no argument reaches it
planned                       has a contract, no implementation
not planned                   explicitly out of scope
```

## Formats

| Format | Extensions | Status | Milestone |
| --- | --- | --- | --- |
| GeoTIFF, classic | `.tif`, `.tiff` | implemented | M2 |
| BigTIFF | `.tif`, `.tiff` | implemented | M2 |
| COG | `.tif`, `.tiff` | planned | M9, as an optimization of the GeoTIFF reader |
| PNG / JPEG + world file | `.png`, `.jpg` | not planned yet | candidate |
| NetCDF, GRIB | `.nc`, `.grib2` | not planned yet | candidate |
| Zarr | directory | not planned yet | candidate |
| Any raster writing | — | not planned | deferred until read is stable |

## TIFF structure

| Feature | Status | Milestone | Notes |
| --- | --- | --- | --- |
| Little-endian and big-endian headers | implemented | M2 | |
| Classic TIFF IFD traversal | implemented | M2 | |
| BigTIFF (64-bit offsets) | implemented | M2 | |
| Strip-organized data | implemented | M3 | Initial uncompressed reads whole rows |
| Tile-organized data | implemented | M3 | Initial uncompressed intersecting-tile reads |
| Multiple IFDs / overviews | implemented | M9 | Discovered at M2, used at M9 |
| Planar configuration: chunky | implemented | M3 | Initial uncompressed path |
| Planar configuration: separate | implemented | M3 | Single-band reads select one plane |
| Subfile / mask IFDs | not planned yet | | |

## Compression

| Compression | Status | Milestone |
| --- | --- | --- |
| None | implemented | M3 | Initial UInt16 and Float32 window path |
| Deflate / zlib | implemented | M3 |
| LZW | implemented | M3 |
| PackBits | implemented | M3 |
| Horizontal differencing predictor | implemented | M3 |
| Floating-point predictor | planned | M3 |
| JPEG | not planned yet | image representation milestone |
| ZSTD | not planned yet | |
| WEBP, LERC, JXL | not planned | |

Unsupported compression is a typed diagnostic naming the code, never a silent
empty read.

## Sample formats

| Sample format | Bits | Status | Milestone |
| --- | --- | --- | --- |
| Unsigned integer | 16 | implemented | M3 | Initial window path |
| Unsigned integer | 8, 32 | planned | M3 | |
| Signed integer | 8, 16, 32 | planned | M3 |
| IEEE float | 32 | implemented | M3 | Initial window path |
| IEEE float | 64 | planned | M3 | |
| Unsigned integer | 1, 2, 4 | not planned yet | sub-byte packing |
| Complex | any | not planned | |

## Georeferencing

| Feature | Status | Milestone | Notes |
| --- | --- | --- | --- |
| `ModelPixelScaleTag` + `ModelTiepointTag` | implemented | M2 | The common north-up path |
| `ModelTransformationTag` | implemented | M2 | Takes precedence on conflict |
| Rotated transforms | implemented | M2 | Supported by the transform code from the start |
| South-up (positive Y scale) | implemented | M2 | Handled, recorded |
| `GTRasterTypeGeoKey` (pixel anchoring) | implemented | M2 | Never guessed |
| Projected CRS from EPSG code | implemented | M2 | |
| Projected CRS from WKT | planned | M2 | Preserved; not interpreted |
| Geographic CRS (degrees) | implemented | M2 | Metadata is preserved; metric authoring remains M4 |
| Vertical CRS and datum | planned | M2 | Preserved as metadata only |
| Reprojection | not planned | | Out of scope |
| Vertical datum transformation | not planned | | Out of scope |

## NoData

| Feature | Status | Milestone |
| --- | --- | --- |
| `GDAL_NODATA` tag | implemented, not connected | M2 | Metadata only |
| Per-band NoData | planned | M3 |
| NaN NoData | implemented, not connected | M2 | Metadata only |
| `skip` face policy | planned | M4 |
| `fill` face policy | planned | M4 |
| `keep` face policy | planned | M4 |
| Alpha or mask band as validity | not planned yet | |

## Representations

| Representation | Status | Milestone | Authored |
| --- | --- | --- | --- |
| `metadata` | implemented | M2 | `Scope` with `geo:` and `raster:` metadata |
| `mesh` | implemented | M4 | Initial uncompressed regular grid; plugin path uses band 1 and step 1 |
| `image` | not planned yet | later | Footprint plane and source reference |

## Authored USD

| Feature | Status | Milestone |
| --- | --- | --- |
| `UsdGeomMesh` points, topology, extent | implemented | M4 | Initial regular-grid path |
| Explicit `orientation` and `subdivisionScheme` | implemented | M4 | Initial regular-grid path |
| `geo:` and `raster:` metadata | implemented | M2 | Metadata and initial mesh representations |
| Local origin and source recovery | implemented | M4 | Initial regular-grid path |
| Payload-backed tiles | planned | M6 |
| Root layer with tile hierarchy | planned | M6 |
| Normals | not planned yet | |
| UVs and materials | not planned yet | image representation |
| `UsdVolume` or field types | not planned | |

## File-format arguments

The connected argument surface per milestone is fixed in
[FILE_FORMAT_ARGUMENTS.md](../architecture/FILE_FORMAT_ARGUMENTS.md). An
argument appears there only in the milestone that implements what it reaches.

## Source access

| Source | Status | Milestone |
| --- | --- | --- |
| Local file | implemented | M2 |
| In-memory fixture | planned | M1 |
| Resolver-provided `ArAsset` | planned | M8 |
| HTTP implemented in this repository | not planned | Belongs to `usd-http-resolver` |

## Known limitations

Recorded before implementation, because each is a boundary a user will hit:

- No reprojection. A stage composed from rasters in different CRSs is the
  responsibility of the caller.
- No vertical datum transformation. Elevations are authored in the vertical
  reference the source declares.
- A geographic-CRS source cannot be authored into a metric stage without an
  explicit, lossy choice.
- Mesh authoring has an interactive vertex ceiling in the plugin; large sources
  go through `usd-raster-convert`.
- Strip-organized sources read whole rows, so a narrow window over a striped
  remote source has high I/O amplification. The reader reports it.
- Multi-band composites are read one band per call. RGB authoring waits for the
  image representation.
