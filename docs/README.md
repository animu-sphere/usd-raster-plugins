# USD Raster Plugins documentation

Documentation is organized by responsibility, following the same taxonomy as
`usd-pointcloud-plugins`, so current contracts, procedures, plans, and
historical records do not drift into one another.

When a summary disagrees with the implementation, the implementation wins and
the summary is a documentation bug. When a summary disagrees with
[architecture/WORKSPACE.md](architecture/WORKSPACE.md) about structure, the
workspace contract wins; structural changes must update that contract first.

| Category | Answers | Start here |
| --- | --- | --- |
| [architecture/](architecture/) | How the workspace is structured, which dependency directions are legal, and what each cross-cutting contract requires. | [WORKSPACE.md](architecture/WORKSPACE.md) |
| [reference/](reference/) | What raster input is accepted today and how it maps to USD. | [CAPABILITY_MATRIX.md](reference/CAPABILITY_MATRIX.md), [RASTER_METADATA.md](reference/RASTER_METADATA.md) |
| [guides/](guides/) | How to build, test, install, and redistribute the plugins. | [BUILDING.md](guides/BUILDING.md), [INSTALL.md](guides/INSTALL.md) |
| [compatibility/](compatibility/) | Which OpenUSD and OpenStrata versions are supported. | [OPENUSD.md](compatibility/OPENUSD.md) |
| [roadmap/](roadmap/) | What remains incomplete and in what order it lands. | [README.md](roadmap/README.md) |
| [releases/](releases/) | Immutable records for tagged releases. | [README.md](releases/README.md) |
| [design/](design/) | Why the project is built this way. | [DESIGN_POLICY.md](design/DESIGN_POLICY.md) |
| [adr/](adr/) | Numbered, immutable architecture decision records. | [0001-existing-usd-schemas.md](adr/0001-existing-usd-schemas.md) |
| [contributing/](contributing/) | What a contributor must update alongside a code change. | [MODULE_README_CONTRACT.md](contributing/MODULE_README_CONTRACT.md) |

## The one-sentence version

`usd-raster-plugins` reads raster and grid geospatial sources — GeoTIFF
first — through a windowed, transport-independent reader, and authors them
into existing OpenUSD schemas. It does not implement HTTP, and it does not
read point clouds.

```text
GeoTIFF -> usdGeoTiff -> RasterGrid -> usdRasterAuthoring -> UsdGeomMesh
                              ^
                              |
                     RandomAccessSource
                              ^
                              |
                    ArAsset / ArResolver  (transport lives here, not here)
```

## Reading order for a new contributor

1. [design/DESIGN_POLICY.md](design/DESIGN_POLICY.md) — the standing direction.
2. [architecture/WORKSPACE.md](architecture/WORKSPACE.md) — module identities
   and legal dependency directions.
3. [architecture/COORDINATE_MODEL.md](architecture/COORDINATE_MODEL.md) — the
   part of a raster pipeline that breaks most often.
4. [architecture/RASTER_READER.md](architecture/RASTER_READER.md) — why the
   reader is windowed rather than whole-image.
5. [roadmap/README.md](roadmap/README.md) — what is being built now.

## Cross-repository boundary

| Repository | Owns | Never owns |
| --- | --- | --- |
| `usd-raster-plugins` | Raster formats, raster metadata, CRS/GeoTransform/NoData interpretation, raster sampling, raster tiling, raster USD authoring | Transport, point-cloud readers |
| `usd-pointcloud-plugins` | LAS/LAZ/COPC/PLY, `UsdGeomPoints`, point tiling and LOD | Raster formats, transport |
| `usd-http-resolver` | HTTP/HTTPS, Range requests, block cache, transport metrics | Any format knowledge |

**A FileFormat Plugin does not know about transport. A resolver does not know
about formats.** That boundary is not negotiable; see
[ADR-0002](adr/0002-resolver-owns-transport.md).
