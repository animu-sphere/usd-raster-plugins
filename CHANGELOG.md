# Changelog

All notable changes to this project are documented here.

The project has not tagged a release. The release sequence is in
[the roadmap](docs/roadmap/README.md); the task-level record is
[implementation status](docs/roadmap/implementation-status.md).

## [Unreleased]

### Added

- Repository skeleton for milestone 0: root `CMakeLists.txt` wiring `libs/`,
  with the OpenUSD-free core lane behind `USDRASTER_CORE_ONLY`; `openstrata.toml`,
  `openstrata.scaffold.yaml`, and `openstrata.ci.yaml` pinned to the `cy2026` /
  `usd` runtime; `VERSION`, `README.md`, `LICENSE`, `NOTICE`, and
  `THIRD_PARTY_NOTICES.md`.
- `tools/generate_fixtures.py`, which produces the synthetic GeoTIFFs the
  golden tests use. Fixtures are generated from checked-in code rather than
  committed as binaries, and the output is byte-identical across platforms.
  Fourteen fixtures cover both endiannesses, BigTIFF, striped and tiled
  layouts including tiles that do not divide the image evenly, both pixel
  anchoring conventions, rotated and conflicting georeferencing, absent
  georeferencing, and numeric and NaN NoData.
- `tools/check_core_headers.py`, which fails if an OpenUSD-free core module
  acquires a forbidden include. It enforces invariant 1 of the workspace
  contract as a test rather than as a convention, and catches the cases the
  core build lane cannot — a libtiff or transport include on a machine that
  happens to have those headers installed.
- `usdGeoCore` (milestone 1): `Vec3d`, `GeoBounds`, `CrsDescription`,
  `LocalOrigin`, `TileId`, `CacheKey`, and the `Diagnostic` vocabulary. The
  local origin is quantized so that it does not move when a window changes,
  and the cache key is length-prefixed and type-tagged so two distinct
  requests cannot frame to the same digest.
- `usdRasterCore` (milestone 1): `RasterSize`, `RasterDataType`,
  `RasterWindow` with intersection, clipping, tile subdivision, and overview
  conversion; `RasterGeoTransform` with the forward and inverse affine map,
  `PixelAnchor`, and window bounds; `RasterMetadata`, `RasterBandInfo`,
  `RasterGrid`, `NoDataValue`, `RasterReadOptions`; and the
  `RandomAccessSource` contract with `MemorySource`, `LocalFileSource`, and
  the `RecordingSource` decorator that makes I/O selectivity assertable.

### Fixed

- `docs/architecture/DIAGNOSTICS.md` declared `std::optional<RasterWindow>` on
  a `usdGeoCore` type, which would invert the dependency edge that section 2
  of the workspace contract fixes — `RasterWindow` belongs to `usdRasterCore`,
  which depends on `usdGeoCore`. The diagnostic anchor is now `PixelWindow`, a
  plain rectangle owned by `usdGeoCore`, and `RasterWindow::ToAnchor`
  converts.

### Documentation

- Established the documentation tree: architecture contracts for the workspace,
  coordinate model, reader, representations, file-format arguments, plugin
  adapter, diagnostics, tiling, cache, and resolver boundary.
- Recorded ADR-0001 through ADR-0008: existing USD schemas before custom
  schemas, the resolver owns transport, a windowed raster reader, GeoTIFF
  first, raster to mesh, the local-origin coordinate policy, GDAL is not a core
  dependency, and the preview/converter split.
- Added the roadmap, the milestone breakdown, and the module README contract.
