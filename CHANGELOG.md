# Changelog

All notable changes to this project are documented here.

The project has not tagged a release. The release sequence is in
[the roadmap](docs/roadmap/README.md); the task-level record is
[implementation status](docs/roadmap/implementation-status.md).

## [Unreleased]

- Connected the initial GeoTIFF `representation=mesh` path: an uncompressed
  window is authored as a regular `UsdGeomMesh` with fixed topology, Y-up
  coordinates, a quantized local origin, extent, and conversion metadata.
- Added an OpenUSD-backed 2x2 mesh authoring regression test and integrated it
  into the `ost build` / `ost test` workflow.

## [0.1.0] - 2026-08-27

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

- `usdGeoCore` and `usdRasterCore` now install a usable CMake package.
  `install(TARGETS … EXPORT …)` recorded an export set that nothing wrote out,
  so the install tree contained no `.cmake` file at all and
  `find_package(usdGeoCore)` — the package name both the workspace contract and
  `openstrata.library.yaml` declare — could not resolve. Each library now ships
  `install(EXPORT)` plus a generated `Config` and `ConfigVersion`, with
  `usdRasterCoreConfig.cmake` issuing `find_dependency(usdGeoCore)` so a
  consumer that asks only for `usdRasterCore` still resolves `usdgeo::core`.
- `RasterGrid` no longer throws `std::length_error` for an unallocatable
  extent. `RasterSize::GetPixelCount` saturates so a malformed size stays
  diagnosable, and handing that saturated value to the allocator undid the
  point of it; the grid is now empty instead, with the requested window
  preserved for the diagnostic.
- `RasterReadOptions::ContributeToCacheKey` folds the output type by its stable
  name rather than by its enumerator ordinal. Inserting a `RasterDataType`
  enumerator — which `RasterTypes.h` explicitly anticipates — would otherwise
  renumber the values after it and silently re-point every existing cache entry
  at a different output type.
- `RasterWindow::GetEndX`, `GetEndY`, and `FromOverview` saturate instead of
  wrapping, and `ToOverview` no longer overflows on a near-maximal end
  coordinate. A wrapped coordinate is a small, valid-looking number that every
  later comparison accepts, so an out-of-range window would have read the wrong
  pixels rather than being rejected.
- `RecordingSource::DidReadRange` now agrees with `GetDistinctBytesRead` about
  zero-length ranges: neither a zero-length recorded read nor a zero-length
  query covers any byte. These two accessors are what selectivity assertions
  are written against, so they have to answer consistently.
- `GeoBounds::Center` is guarded like `Size`, returning the origin for an
  invalid extent instead of NaN. `Empty()` is the documented starting point for
  an accumulator, and a NaN centre propagates into a local origin and out to
  every authored position.
- `tools/check_core_headers.py` allowlists the complete C++17 standard library,
  including `<any>`, which was missing. The check is a required gate, so a
  missing entry failed a legitimate include.
- `.github/workflows/core-ci.yml` asserts that all three host manifests arrived
  before diffing them. The comparison loop previously ran zero iterations and
  still reported success when fewer than two manifests were present — vacuous
  in exactly the case a missing upload should have failed the gate.
- `openstrata.ci.yaml`'s commented bundle block no longer re-declares a
  top-level `cells:` key. Following its own activation instruction would have
  produced a duplicate YAML mapping key, which either errors or silently
  discards the six workspace cells.
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
