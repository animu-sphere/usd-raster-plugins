# Milestone 0: Repository Skeleton

## Objective

Establish the structure, contracts, and verification path before any raster
code exists, so that the first implementation lands into a shape that already
enforces the boundaries.

## Deliverables

- `CMakeLists.txt` at the root wiring `libs/`, `plugins/`, and `tools/`, with
  an OpenUSD-free core lane that builds and tests without an OpenUSD runtime.
- `openstrata.toml`, `openstrata.scaffold.yaml`, and `openstrata.ci.yaml`
  matching the pinned `cy2026` / `usd` runtime.
- `VERSION`, `CHANGELOG.md`, `README.md`, `LICENSE`, `NOTICE`, and
  `THIRD_PARTY_NOTICES.md`.
- The documentation tree in `docs/`, with the architecture contracts and the
  eight initial ADRs.
- The module README contract, applied to every module directory created from
  Milestone 1 onward.
- CI generated from `openstrata.ci.yaml` for Windows, Linux, and macOS arm64.
- A fixture-generation script producing the synthetic GeoTIFFs the golden tests
  will use, checked in as code rather than as binaries.

## Exit criteria

- The root CMake build configures and runs an empty test suite on Windows,
  Linux, and macOS.
- `ost configure`, `ost build`, and `ost test` succeed on the pinned runtime.
- The core lane builds with no OpenUSD available.
- Every document in `docs/` links only to documents that exist.
- The fixture script produces byte-identical output on all three platforms.

## Decisions to settle here

Settled, and recorded as ADRs:

- Existing USD schemas before custom schemas — [ADR-0001](../adr/0001-existing-usd-schemas.md)
- The resolver owns transport — [ADR-0002](../adr/0002-resolver-owns-transport.md)
- Windowed, random-access reader — [ADR-0003](../adr/0003-windowed-raster-reader.md)
- GeoTIFF first — [ADR-0004](../adr/0004-geotiff-first-format.md)
- Raster to mesh — [ADR-0005](../adr/0005-raster-to-mesh.md)
- Local-origin coordinate policy — [ADR-0006](../adr/0006-local-origin-coordinate-policy.md)
- GDAL was excluded from production for Milestone 0 —
  [ADR-0007](../adr/0007-gdal-not-a-core-dependency.md), superseded by the
  optional production backend decision in
  [ADR-0009](../adr/0009-gdal-raster-backend.md)
- Preview path versus converter path — [ADR-0008](../adr/0008-preview-vs-converter.md)

Deliberately deferred, with the milestone that resolves each:

| Question | Resolved at |
| --- | --- |
| libgeotiff versus a minimal in-repository GeoTIFF key decoder | Milestone 2, recorded as its own ADR |
| Whether PROJ is introduced, and where its boundary sits | after Milestone 4, with real CRS cases |
| The interactive vertex ceiling value | Milestone 4, measured rather than guessed |
| The `lod` profile step values | Milestone 5, from measurement on real DEMs |
| Generated-cache layout details | Milestone 7 |

## Environment to pin

- OpenStrata target `cy2026`, profile `usd`.
- OpenUSD 26.08; plugin manifests declare `>=26.08,<27.0`.
- libtiff version pinned once Milestone 2 selects it, with license obligations
  recorded in [DISTRIBUTION.md](../guides/DISTRIBUTION.md).
- Compilers: MSVC on Windows, Clang on macOS, GCC on Linux, at the versions the
  pinned runtime supplies.

## Verification path

```text
ost configure
ost build
ost test
```

plus the plain-CMake core lane:

```text
cmake -S . -B build -DUSDRASTER_CORE_ONLY=ON
cmake --build build
ctest --test-dir build
```

The second must work with no OpenUSD present. If it stops working, a core
library has acquired an OpenUSD dependency, which is invariant 1 of the
[workspace contract](../architecture/WORKSPACE.md) failing.

## Outstanding

- Confirm that the pinned runtime supplies a usable libtiff, and record the
  fallback plan if it does not.
- Decide whether ASan and TSan lanes run per pull request or nightly.
- Choose the fixture-generation language. Python is the default, matching the
  release tooling of `usd-pointcloud-plugins`.
