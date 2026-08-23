# OpenUSD Raster Plugins

OpenUSD FileFormat Plugins and libraries for raster and grid geospatial data.
The project reads GeoTIFF first, through a windowed, transport-independent
reader, and authors the result into existing OpenUSD schemas.

**What it does**

- Reads raster sources through a windowed reader that never requires
  materializing a whole image to satisfy a bounded request.
- Keeps georeferencing — CRS, geotransform, pixel anchoring, NoData — explicit
  at every boundary that converts between pixel, source, and stage-local
  coordinates.
- Authors `UsdGeomMesh` elevation and geospatial metadata through existing
  OpenUSD schemas, with no custom schema of its own.
- Implements no transport. Remote access composes at runtime through the active
  OpenUSD `ArResolver`.

```text
GeoTIFF -> usdGeoTiff -> RasterGrid -> usdRasterAuthoring -> UsdGeomMesh
                              ^
                              |
                     RandomAccessSource
                              ^
                              |
                    ArAsset / ArResolver  (transport lives here, not here)
```

## Status

Early. The repository structure, core libraries, GeoTIFF metadata reader, and
metadata-only plugin are implemented and tested. Pixel decoding and mesh
authoring remain planned. No release has been tagged.

| Milestone | Scope | Status |
| --- | --- | --- |
| 0 | Repository skeleton, CMake, OpenStrata workspace, CI, docs | done |
| 1 | Raster core value model, with no OpenUSD | done |
| 2 | GeoTIFF metadata and a metadata-only FileFormat Plugin | in progress |
| 3 | Windowed pixel reading | planned |
| 4 | `UsdGeomMesh` authoring | planned |

The task-level record is
[implementation status](docs/roadmap/implementation-status.md); source support
is [the capability matrix](docs/reference/CAPABILITY_MATRIX.md). When a summary
disagrees with the implementation, the implementation wins.

## Supported formats

| Extension | Plugin | Current support |
| --- | --- | --- |
| `.tif`, `.tiff` | `raster-geotiff` | metadata representation — milestone 2 |

`.tif` and `.tiff` are shared with non-geospatial TIFF use. The bundle claims
them, and a source without usable georeferencing is reported rather than
assigned an invented CRS.

## Scope boundary

A FileFormat Plugin does not know about transport, and a resolver does not know
about formats. That boundary is not negotiable; see
[ADR-0002](docs/adr/0002-resolver-owns-transport.md).

| Repository | Owns |
| --- | --- |
| `usd-raster-plugins` | Raster formats, raster metadata, CRS/geotransform/NoData interpretation, raster sampling and tiling, raster USD authoring |
| `usd-pointcloud-plugins` | LAS/LAZ/COPC/PLY, `UsdGeomPoints`, point tiling and LOD |
| `usd-http-resolver` | HTTP/HTTPS, Range requests, block cache, transport metrics |

Reprojection, vertical datum transformation, raster writing, and
renderer-specific material graphs are out of scope; see
[the roadmap](docs/roadmap/README.md).

## Building

The OpenStrata path, against the pinned `cy2026` / `usd` runtime:

```text
ost configure
ost build
ost test
```

The core lane, which must build and test with no OpenUSD present:

```text
cmake -S . -B build-core -DUSDRASTER_CORE_ONLY=ON
cmake --build build-core
ctest --test-dir build-core
```

If the core lane stops working, a core library has acquired an OpenUSD
dependency, which is invariant 1 of
[the workspace contract](docs/architecture/WORKSPACE.md) failing. Details are
in [BUILDING.md](docs/guides/BUILDING.md).

## Documentation

Start at [docs/README.md](docs/README.md). The reading order for a new
contributor is the design policy, the workspace contract, the coordinate model,
the reader contract, and then the roadmap.

## License

Apache-2.0. See [LICENSE](LICENSE), [NOTICE](NOTICE), and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
