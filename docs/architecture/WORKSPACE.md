# Workspace contract

This is the binding structural contract for `usd-raster-plugins`. It fixes
module identities, dependency directions, root responsibilities, artifact
naming, and change invariants. A structural change that contradicts this
document must change this document first.

Status is recorded per component below and at task level in
[implementation status](../roadmap/implementation-status.md). The table records
ownership boundaries that apply from the first commit of each module, not a
requirement to scaffold empty directories. A directory is created when its
first tested capability is implemented.

## 1. Components

| Identity | Directory | Kind | Status | Responsibility |
| --- | --- | --- | --- | --- |
| `usdGeoCore` | `libs/usd-geo-core` | plain CMake/OpenStrata static library | implemented | Format-independent geospatial values: CRS-related values, linear/vertical units, affine transforms, local-origin transforms, spatial bounds, deterministic tile IDs, normalized cache-key inputs, source identity, and the typed diagnostic vocabulary shared across modules. |
| `usdRasterCore` | `libs/usd-raster-core` | plain CMake/OpenStrata static library | implemented | Format-independent raster contracts: `RasterSize`, `RasterWindow`, `RasterDataType`, `RasterBandInfo`, `RasterGeoTransform`, `RasterMetadata`, `RasterGrid`, `RasterTile`, `RasterReadOptions`, `RasterSample`, NoData policy, resampling and sampling-step definitions, and the `RandomAccessSource` byte-source contract. |
| `usdGeoTiff` | `libs/usd-geotiff` | plain CMake/OpenStrata static library | implemented | GeoTIFF metadata and window reading behind `GeoTiffReader`: header and IFD traversal, strip/tile layout, sample format, BigTIFF, GeoTIFF keys, geotransform, CRS basics, NoData, band metadata, and decoded windows. The optional libtiff backend provides compressed decoding behind the same reader boundary. |
| `usdRasterGdal` | `libs/usd-raster-gdal` | optional plain CMake/OpenStrata static library | planned | Preferred general raster backend: maps GDAL datasets, metadata, bands, overviews, and bounded window reads into `usdRasterCore` contracts. Owns the project VSI adapter over `RandomAccessSource`; never exposes GDAL types or uses GDAL transport drivers from plugin code. |
| `usdRasterTiling` | `libs/usd-raster-tiling` | plain CMake/OpenStrata static library | planned | Format-independent spatial partitioning: source-tile to spatial-tile mapping, deterministic tile ordering and identity, bounded-memory window planning, tile manifest serialization, and overview/level planning. Contains no format parsing and no OpenUSD types. |
| `usdRasterAuthoring` | `libs/usd-raster-authoring` | plain CMake/OpenStrata static library | implemented | Shared OpenUSD authoring; currently geospatial metadata and bounded regular-grid mesh authoring. Image/texture representation, payload-backed tile assets, root layer generation, and broader validation remain planned. The only library that includes OpenUSD headers. |
| `usdGeoCache` | `libs/usd-geo-cache` | plain CMake/OpenStrata static library | deferred | Descriptor-based stable cache keys, deterministic generated-USDC layout, lookup states, and entry invalidation for generated representations. Introduced when the converter needs it; see [CACHE.md](CACHE.md). |
| `raster-geotiff` | `plugins/raster-geotiff` | OpenStrata plugin bundle (`usd-fileformat`) | implemented | OpenUSD `SdfFileFormat` adapter for `.tif` / `.tiff`: plugin registration, argument normalization, `ArAsset` adaptation, `GeoTiffReader` construction, and metadata authoring through the shared library. Owns its `GTIFxxx` diagnostic codes. |
| `usd-raster-info` | `tools/usd-raster-info` | CLI executable | planned | Backend-independent metadata inspection: format, dimensions, bands, data types, CRS, bounds, NoData, and pixel resolution. |
| `usd-raster-convert` | `tools/usd-raster-convert` | CLI executable | planned | Explicit, long-running, deterministic conversion with independent `mesh` and `heightmap` modes: tiled payload generation, image output, manifests, generated cache population, and resumable batch workflows. |

Point-cloud contracts belong to `usd-pointcloud-plugins`; transport belongs to
`usd-http-resolver`. Neither is a reserved module in this repository.

None of the `libs/` modules is a plugin: none has a `plugInfo.json`, none
performs plugin registration, and only `usdRasterAuthoring` exposes OpenUSD
types.

`.tif` and `.tiff` are shared with non-geospatial TIFF use. The bundle claims
them, and a source without usable georeferencing is reported rather than
assigned an invented CRS; explicit arguments are the sanctioned way to supply
what the file does not carry.

## 2. Dependency directions

Allowed:

```text
usdRasterCore       -> usdGeoCore
usdGeoCache         -> usdGeoCore
usdGeoTiff          -> usdGeoCore, usdRasterCore
usdGeoTiff          -> libtiff (private, decoding backend)
usdRasterGdal       -> usdGeoCore, usdRasterCore, GDAL (private backend)
usdRasterTiling     -> usdGeoCore, usdRasterCore
usdRasterAuthoring  -> usdGeoCore, usdRasterCore
usdRasterAuthoring  -> OpenUSD (sdf, usd, usdGeom, usdShade)
raster-geotiff      -> usdGeoTiff, usdRasterTiling, usdRasterAuthoring, OpenUSD
usd-raster-convert  -> usdGeoTiff, usdRasterTiling, usdRasterAuthoring, usdGeoCache
```

Reserved future directions:

```text
any raster reader   -> usdGeoCore, usdRasterCore
any raster bundle   -> usdRasterAuthoring
```

Forbidden:

```text
usdGeoCore          -> anything (OpenUSD, a reader, a codec, libtiff, PROJ, GDAL)
usdRasterCore       -> OpenUSD, TIFF/GeoTIFF types, libtiff
usdGeoTiff          -> OpenUSD, tiling policy, payload generation,
                       plugin registration, HTTP
usdRasterGdal       -> OpenUSD, authoring policy, plugin registration, HTTP;
                       GDAL /vsicurl/ and cloud transport drivers
usdRasterTiling     -> TIFF parsing, OpenUSD, plugin registration
usdRasterAuthoring  -> TIFF decoding, plugin argument parsing,
                       renderer implementation
any plugin bundle   -> another plugin bundle
any module          -> an HTTP client, a cloud SDK, or a resolver implementation
any dependency cycle
```

The readers must not depend on the tiling or OpenUSD authoring modules, and the
tiling module must not depend on GeoTIFF. The plugin adapter and the converter
are what compose readers, tiling, and OpenUSD authoring.

Each `libs/*/openstrata.library.yaml` gives the plain library a workspace
identity and CMake package/target. A bundle declares the edge in its manifest;
`ost plugin build/test/package` resolves and executes it.

| Identity | CMake package | CMake target | C++ namespace |
| --- | --- | --- | --- |
| `usdGeoCore` | `usdGeoCore` | `usdgeo::core` | `usdgeo` |
| `usdRasterCore` | `usdRasterCore` | `usdraster::core` | `usdraster` |
| `usdGeoTiff` | `usdGeoTiff` | `usdgeotiff::core` | `usdgeotiff` |
| `usdRasterGdal` | `usdRasterGdal` | `usdraster::gdal` | `usdraster::gdal` |
| `usdRasterTiling` | `usdRasterTiling` | `usdraster::tiling` | `usdraster` |
| `usdRasterAuthoring` | `usdRasterAuthoring` | `usdraster::authoring` | `usdraster` |
| `usdGeoCache` | `usdGeoCache` | `usdgeo::cache` | `usdgeo` |
| `raster-geotiff` | `UsdRasterGeoTiffFileFormat` | — | `usdrastertiff` |

Directory names, CMake target names, and C++ namespaces are deliberately not
required to be identical: the external bundle name uses the explicit `raster`
term while internal C++ prefixes stay short.

## 3. Source boundaries

```text
plugins/raster-geotiff/src/UsdRasterGeoTiffFileFormat.cpp
    thin SdfFileFormat integration: argument normalization, ArAsset
    adaptation, reader call, authoring call, diagnostic projection

plugins/raster-geotiff/include/usdrastertiff/UsdRasterTiffDiagnostics.h
    the stable GTIFxxx codes of the bundle

libs/usd-geotiff/
    TIFF header and IFD traversal, strip/tile layout, decompression,
    GeoTIFF key decoding, geotransform construction, GeoTiffReader
    orchestration, libtiff isolation

libs/usd-raster-core/
    the raster value model, window and tile contracts, read options,
    sampling, NoData policy, and the RandomAccessSource interface every
    reader consumes

libs/usd-geo-core/
    format- and USD-independent geospatial values and diagnostics

libs/usd-raster-tiling/
    spatial partitioning and bounded-memory window planning

libs/usd-raster-authoring/
    OpenUSD authoring, shared by every raster bundle and by the converter
```

Plugin C++ sources, `plugInfo.json`, format fixtures, and the per-bundle
diagnostic tables belong to the bundle. Each bundle must remain buildable
through `ost` as an independent bundle; the root CMake build is an additional
supported path.

## 4. Root responsibilities

The repository root owns composition, not module implementation:

- the plain CMake build that wires `libs/`, `plugins/`, and `tools/` together;
- workspace-wide version and OpenStrata platform/profile selection;
- `openstrata.ci.yaml` and generated CI;
- shared licensing, third-party notices, documentation, and release records;
- cross-representation equivalence tests and aggregate packaging.

## 5. Authored stage contract

The authored shape depends on the selected representation. For
`representation=mesh` with `lod=off` and no tiling:

```text
/Raster                 UsdGeomMesh
```

For `representation=metadata`:

```text
/Raster                 Scope, geospatial metadata only, no pixel data read
```

The stage is Y-up with one meter per unit. Mesh positions are stage-local
`float` values relative to `geo:localOrigin`; source coordinates are recovered
through that origin. Spatial partitioning uses source coordinates while
authoring uses stage-local coordinates, so tile identity stays stable across
up-axis conversion.

The exact metadata authored on each prim is fixed in
[RASTER_METADATA.md](../reference/RASTER_METADATA.md); the shapes the
authoring library can produce are fixed in
[REPRESENTATIONS.md](REPRESENTATIONS.md) and [TILING.md](TILING.md).

## 6. Artifact naming and versioning

Per-bundle artifacts use the target-qualified convention of OpenStrata:

```text
raster-geotiff-<version>-<target>.tar.zst
```

The installed shared library is `UsdRasterGeoTiffFileFormat`, and the
registered `plugInfo.json` type name matches it. Until a real need for
independent release cadences appears, every bundle and plain library mirrors
the repository-root `VERSION`. Git tags use `vX.Y.Z`.

## 7. Change invariants

Every structural or format change preserves these invariants:

1. Public APIs use project-owned value types and standard-library types.
2. Source coordinates and stage-local coordinates use distinct names and
   explicit transforms.
3. Pixel anchoring (`pixel-is-area` or `pixel-is-point`) is explicit at every
   boundary that converts between pixel and world coordinates. See
   [COORDINATE_MODEL.md](COORDINATE_MODEL.md).
4. Readers return deterministic, validated intermediate data and never author
   USD directly.
5. The reader surface is windowed. No public API requires materializing a
   whole image to satisfy a bounded request. See
   [RASTER_READER.md](RASTER_READER.md).
6. File-format arguments are normalized before reader or cache lookup, and
   normalized arguments participate in layer identity. See
   [FILE_FORMAT_ARGUMENTS.md](FILE_FORMAT_ARGUMENTS.md).
7. Unknown source metadata is preserved or reported; it is never silently
   discarded.
8. Lossy steps — resampling, NoData substitution, quantization — are recorded
   in authored metadata.
9. Spatial tiling and level of detail stay separate concepts; neither collapses
   into the other.
10. Hydra, cameras, viewport state, and screen-space math never appear in a
    reader, a tiling module, or a plugin adapter.
11. A new format reaches USD through the shared raster contracts and the shared
    authoring entry point, not by copying GeoTIFF assumptions into a new
    writer.
12. Manifest and CMake dependency declarations change together.
13. Plugin registration changes include a discovery test.
14. Third-party revision and license changes update both notices and package
    verification.
15. A change that modifies a module contract updates the `README.md` of that
    module in the same pull request. See
    [MODULE_README_CONTRACT.md](../contributing/MODULE_README_CONTRACT.md).
16. Write support is deferred until read behavior and preservation rules are
    stable.
17. No resolver implementation is a build-time dependency. The workspace
    contains no CMake dependency, submodule, vendored transport library,
    resolver-specific include, or link dependency on one; external resolvers
    compose at runtime. See [RESOLVER_SOURCE.md](RESOLVER_SOURCE.md).
18. Transport-specific concepts do not enter shared contracts. Resolver
    validation tokens are opaque, and credentials, authorization headers,
    signed URLs, and tokens are never persisted into manifests, cache
    descriptors, or diagnostics.
19. GDAL is private to the optional `usdRasterGdal` production module and to
    oracle tests. It never enters core headers, plugin APIs, or transport
    policy. See [ADR-0009](../adr/0009-gdal-raster-backend.md).

## 8. Build and packaging

- Use OpenStrata manifests for libraries and plugin bundles.
- Keep the root CMake build working without `ost` for local development.
- Keep large dependencies optional and scoped to the owning target.
- Test pure libraries without requiring an OpenUSD runtime: `usdGeoCore`,
    `usdRasterCore`, `usdGeoTiff`, `usdRasterGdal`, `usdRasterTiling`, and
    `usdGeoCache` build and test with no OpenUSD runtime; the dependency-free
    core lane may omit optional codec backends. `usdRasterAuthoring`, the plugin bundle, and
  the converter require one.
- Validate plugin bundles with the pinned OpenStrata `cy2026` / `usd` runtime.

## 9. CI and verification contract

`openstrata.ci.yaml` is the source of truth; the GitHub workflow is generated
by `ost ci generate github`. The planned PR matrix runs the production bundle
on every host, and a separate OpenUSD-free lane builds and tests the core
libraries with plain CMake:

| Host | Target | OST level |
| --- | --- | --- |
| Windows 2022 x86_64 | cy2026 / USD | L0-L4 |
| macOS 15 arm64 | cy2026 / USD | L0-L5 |
| Ubuntu 24.04 x86_64 | cy2026 / USD | L0-L5 |
| All three | core-only, plain CMake | `cmake`, `build`, `ctest` |

The planned required local gate is:

```text
ost configure
ost build
ost test
ost plugin build plugins/raster-geotiff
ost plugin test plugins/raster-geotiff --up-to 4
```

ASan and TSan lanes are added on Linux and macOS where the pinned runtime
allows them.

The gate must stay passable without any external resolver repository.
Repository-local resolver contract tests are the required CI gate, and
cross-repository integration against an external resolver implementation is
composed separately; see [RESOLVER_SOURCE.md](RESOLVER_SOURCE.md).

## 10. Delivery status

| Milestone | Boundary | Status |
| --- | --- | --- |
| v0.1.0 | repository structure, raster core, GeoTIFF metadata, metadata-only FileFormat Plugin | released |
| v0.2.0 | band reads, `RasterWindow`, strip and tile layouts, NoData | released |
| v0.3.0 | DEM to `UsdGeomMesh`, coordinate transform, local origin, format arguments | planned |
| v0.4.0 | bounded-memory tiling and payload-backed authoring | planned |
| v0.5.0 | production converter, manifests, generated cache | planned |
| v0.6.0 | resolver-backed GeoTIFF reads | planned |
| v0.7.0 | COG-aware optimization and performance baseline | planned |

Current work and acceptance gaps are tracked in
[roadmap/implementation-status.md](../roadmap/implementation-status.md).
