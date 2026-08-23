# Building and testing

Planned procedure. Nothing is implemented yet, so the commands below describe
the intended build surface rather than a working one. They are recorded now
because Milestone 0 delivers exactly this.

Two build paths are supported, deliberately:

```text
OpenStrata (ost)   the primary path; plugin bundles and the pinned runtime
plain CMake        core libraries without OpenUSD, for fast local iteration
```

Both must keep working. The plain-CMake core lane is what proves that the core
libraries have not acquired an OpenUSD dependency.

## OpenStrata path (primary)

Requires the pinned OpenStrata runtime: target `cy2026`, profile `usd`.

```bash
ost configure
ost build
ost test
```

Per-bundle:

```bash
ost plugin build plugins/raster-geotiff
ost plugin test  plugins/raster-geotiff --up-to 4
```

The `--up-to 4` levels are the discovery, build, `usdcat.read`, and
`python.stage_open` checks. Level 5 adds the runtime smoke checks that the
Linux and macOS cells run.

## Core-only path (no OpenUSD)

```bash
cmake -S . -B build-core -DUSDRASTER_CORE_ONLY=ON
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

This builds `usdGeoCore`, `usdRasterCore`, `usdGeoTiff`, `usdRasterTiling`, and
`usdGeoCache` with their tests, and nothing that includes an OpenUSD header.

If this fails to configure because OpenUSD is missing, a core library has
gained a dependency it must not have. That is a workspace-contract violation,
not a build-configuration problem. See
[WORKSPACE.md](../architecture/WORKSPACE.md).

## Full plain-CMake path

```bash
cmake -S . -B build -Dpxr_DIR=<usd-install>/lib/cmake/pxr
cmake --build build
ctest --test-dir build --output-on-failure
```

Builds everything, including `usdRasterAuthoring`, the plugin bundle, and the
converter.

## Test fixtures

Golden fixtures are generated, not committed as opaque binaries:

```bash
python tools/generate_fixtures.py --out tests/fixtures
```

The script must produce byte-identical output on Windows, Linux, and macOS. A
fixture is reviewable as code, which matters most for the malformed inputs that
exercise diagnostics — a hand-corrupted binary tells a reviewer nothing about
what it is testing.

## Test layers

```bash
# core: no OpenUSD, milliseconds
ctest --test-dir build-core

# golden: authored output compared byte-for-byte
ctest --test-dir build -R golden

# oracle: optional, requires GDAL
cmake -S . -B build -DUSDRASTER_ENABLE_GDAL_ORACLE=ON
ctest --test-dir build -R oracle

# plugin and resolver: requires an OpenUSD runtime
ost plugin test plugins/raster-geotiff --up-to 4
```

Oracle tests are optional by construction and are skipped when GDAL is not
present, so the required gate never depends on it. See
[ADR-0007](../adr/0007-gdal-not-a-core-dependency.md).

## Optional libtiff backend

The core lane does not discover codecs. Enable the GeoTIFF backend explicitly
when configuring a build with a system-provided libtiff:

```bash
cmake -S . -B build-libtiff -DUSDRASTER_ENABLE_LIBTIFF=ON
```

The dependency is exposed through the repository-owned
`usd-raster::libtiff` target. A vendored libtiff source tree can replace the
adapter later without changing `usdGeoTiff`; its version and license must be
recorded before it is used in a distributed artifact. GDAL remains test-only
until the format-breadth decision gate in
[format support order](../roadmap/format-support-order.md) selects an
optional breadth module.

## Converting a raster explicitly

```bash
usd-raster-convert \
  terrain.tif \
  Terrain.usda \
  --representation mesh \
  --lod balanced \
  --tile-size 512
```

The converter is the production path. A FileFormat read of a large source
fails with a diagnostic naming this command rather than running for minutes
inside a host. See [ADR-0008](../adr/0008-preview-vs-converter.md).

## Previewing a file

```bash
usdview terrain.tif

# with arguments
usdview 'terrain.tif:SDF_FORMAT_ARGS:representation=mesh&lod=preview'
```

The default representation is `metadata`, so opening a large or remote raster
is cheap. Producing geometry is an explicit request.

## Sanitizers

```bash
cmake -S . -B build-asan -DUSDRASTER_CORE_ONLY=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
```

The core libraries are the highest-value sanitizer target: they parse untrusted
binary input, which is where an offset arithmetic bug becomes a security
problem rather than a rendering problem.

## Local gate before a pull request

```text
ost configure
ost build
ost test
ost plugin build plugins/raster-geotiff
ost plugin test  plugins/raster-geotiff --up-to 4
cmake -S . -B build-core -DUSDRASTER_CORE_ONLY=ON && ctest --test-dir build-core
```

## CI

`openstrata.ci.yaml` is the source of truth; the GitHub workflow is generated
by `ost ci generate github`. The matrix is fixed in section 9 of
[WORKSPACE.md](../architecture/WORKSPACE.md).
