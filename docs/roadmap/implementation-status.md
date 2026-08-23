# Implementation Status

The task-level record of what exists. The
[capability matrix](../reference/CAPABILITY_MATRIX.md) records source support;
this file records work.

**Current state: milestones 0 and 1 are complete, and the GeoTIFF metadata
vertical slice is connected.** The repository skeleton, the OpenUSD-free core
lane, the core libraries, the metadata authoring library, and the first
format-specific plugin are built and tested. Pixel decoding, mesh authoring,
and converter sources are not implemented yet, and no release has been tagged.

Status words, from
[MODULE_README_CONTRACT.md](../contributing/MODULE_README_CONTRACT.md):

```text
implemented                   in this module, tested
implemented, not connected    exists in a library, no argument reaches it
planned                       has a contract, no implementation
not planned                   explicitly out of scope
```

## Milestone 0 — repository skeleton (done)

| Task | Status |
| --- | --- |
| Documentation taxonomy under `docs/` | done |
| Architecture contracts: workspace, coordinates, reader, representations, arguments, adapter, diagnostics, tiling, cache, resolver | done |
| ADR-0001 through ADR-0008 | done |
| Roadmap and milestone breakdown | done |
| Module README contract | done |
| Root `CMakeLists.txt` and core-only lane | done |
| `openstrata.toml`, `openstrata.scaffold.yaml`, `openstrata.ci.yaml` | done |
| `VERSION`, `CHANGELOG.md`, root `README.md` | done |
| `LICENSE`, `NOTICE`, `THIRD_PARTY_NOTICES.md` | done |
| GitHub workflow: core lane | done |
| GitHub workflow: generated `ost` lane | blocked, see below |
| Fixture-generation script | done |
| Core dependency check enforcing invariant 1 | done |

Three deviations from the milestone-0 plan, all recorded here rather than left
implicit:

- **`openstrata.ci.yaml` keeps workspace and bundle cells separate.** The six
  workspace cells build the root CMake tree and run its CTest suite on all
  three hosts. Six bundle cells now build and test `plugins/raster-geotiff`;
  Windows runs through level 4 and Linux/macOS run through level 5.
- **The core-only lane is a hand-written workflow.** `ost ci generate` cannot
  express a lane that materializes no runtime SDK at all, which is exactly what
  that lane is for, so `.github/workflows/core-ci.yml` is maintained by hand.
- **The generated `ost` workflow is committed.** `ost ci generate github`
  renders the 12-cell matrix, including the bundle discovery and stage-open
  checks. The core-only lane remains the hand-written workflow described above.

Exit criteria are in
[phase-0-repository-skeleton.md](phase-0-repository-skeleton.md).

## Milestone 1 — raster core (done)

| Task | Status |
| --- | --- |
| `usdGeoCore` value types and diagnostics | implemented |
| `usdRasterCore` raster value model | implemented |
| Affine transform, forward and inverse | implemented |
| Window arithmetic and tile subdivision | implemented |
| NoData representation and comparison | implemented |
| `RandomAccessSource`, `MemorySource`, `LocalFileSource` | implemented |
| Instrumented source test double (`RecordingSource`) | implemented |
| Precision round-trip tests | implemented |

Exit criteria, and how each is verified:

| Criterion | Verified by |
| --- | --- |
| Builds and tests with no OpenUSD available | `core-ci.yml` on all three hosts |
| No header includes OpenUSD, libtiff, or transport | `core_dependency_check`, which also fails on a planted violation |
| Pixel-to-source correct for both anchorings under north-up, south-up, and rotated transforms | hand-computed values in `test_raster_core.cpp` |
| A window subdivides and reassembles with no gap or overlap, for sizes that do not divide evenly | `TestWindowSubdivideCoversExactly`, which claims every pixel exactly once |
| The instrumented source proves a bounded read requests only what it needs | `TestRecordingSourceProvesSelectivity` |

Two questions the milestone left open are now answered:

- **`RasterGrid` owns its buffer.** Owning makes the lifetime rule statable in
  one sentence; a non-owning view is added only if a measured copy cost
  justifies it.
- **Sub-byte sample formats stay out of `RasterDataType`.** An unused
  enumerator is a contract nothing tests.

Detail in [phase-1-raster-core.md](phase-1-raster-core.md).

## Milestone 2 — GeoTIFF metadata (in progress, `v0.1.0`)

| Task | Status |
| --- | --- |
| TIFF header, both endiannesses, classic and BigTIFF | implemented |
| IFD traversal and tag validation | implemented |
| Strip and tile layout description | implemented |
| Overview discovery | implemented |
| GeoTIFF key decoding | implemented |
| `ModelPixelScale` / `ModelTiepoint` / `ModelTransformation` | implemented |
| GDAL NoData and sample metadata | implemented |
| Band descriptions, units, scale, and offset | planned |
| `representation=metadata` authoring | implemented |
| Plugin registration and discovery test | implemented |
| `GTIF001`-`GTIF009` with a fixture each | planned |
| `third_party/libtiff` dependency target and optional system discovery | implemented, not connected |
| ADR: libgeotiff versus in-repository key decoding | open |

## Milestone 3 — pixel reading (planned, `v0.2.0`)

| Task | Status |
| --- | --- |
| Strip and tile decoding | planned |
| Deflate, LZW, PackBits, predictors | planned |
| Integer and float sample formats | planned |
| Chunky and separate planar configuration | planned |
| `ReadWindow`, `ReadTile`, `ReadScanlines` | planned |
| Read planning and range coalescing | planned |
| I/O counters and amplification reporting | planned |
| Memory budget enforcement | planned |
| Cancellation at tile boundaries | planned |

## Milestone 4 — mesh authoring (planned, `v0.3.0`)

| Task | Status |
| --- | --- |
| `usdRasterAuthoring` regular grid mesh | planned |
| Full coordinate chain and axis mapping | planned |
| Local origin computation and authoring | planned |
| NoData policies | planned |
| `geo:` and `raster:` metadata authoring | planned |
| Golden topology and position fixtures | planned |
| Interactive vertex ceiling, `GTIF012` | planned |
| Vertex-count measurement across platforms | planned |

## Milestone 5 — file-format arguments (planned, `v0.3.0`)

| Task | Status |
| --- | --- |
| Argument parsing and normalization | planned |
| Shared-layer validation | planned |
| Dynamic file-format registration | planned |
| Profile-to-step table | planned |
| Layer identity and normalization tests | planned |

## Milestone 6 — tiling (planned, `v0.4.0`)

| Task | Status |
| --- | --- |
| `usdRasterTiling` spatial planning | planned |
| Deterministic tile identity | planned |
| Shared-edge tile authoring | planned |
| Level construction | planned |
| Payload-backed tile assets and root layer | planned |
| Tile manifest | planned |
| Bounded-memory conversion loop | planned |

## Milestone 7 — converter (planned, `v0.5.0`)

| Task | Status |
| --- | --- |
| `usd-raster-convert` CLI | planned |
| Deterministic byte-stable output | planned |
| Manifests with I/O and memory counters | planned |
| `usdGeoCache` and atomic publication | planned |
| Resumability | planned |
| Preview/production equivalence test | planned |

## Milestone 8 — resolver interoperability (planned, `v0.6.0`)

| Task | Status |
| --- | --- |
| `ArAssetSource` adapter | planned |
| Local reads moved onto the same interface | planned |
| Source identity and conservative reuse | planned |
| In-repository test resolver fixture | planned |
| Three-way equivalence test | planned |
| Selectivity assertions | planned |

## Milestone 9 — COG optimization (planned, `v0.7.0`)

| Task | Status |
| --- | --- |
| Overview selection | planned |
| Tile-aware read planning | planned |
| Remote selectivity metrics | planned |
| Performance baseline on real data | planned |

## Open questions

Tracked here until an ADR or a milestone resolves them.

| Question | Owner milestone | State |
| --- | --- | --- |
| libgeotiff versus a minimal in-repository GeoTIFF key decoder | 2 | open |
| Whether PROJ is introduced, and where its boundary sits | after 4 | open |
| Whether format breadth justifies an optional GDAL reader | before the first post-COG format | open: use the format-breadth decision gate |
| The interactive vertex ceiling value | 4 | open |
| The `lod` profile step values | 5 | open |
| Whether `RasterGrid` owns or views its buffer | 1 | resolved: it owns |
| Whether sub-byte sample formats enter `RasterDataType` | deferred | resolved: not yet |
| How a geographic-CRS source is handled in a metric stage | 4 | open |
| Whether ASan and TSan run per pull request or nightly | 0 | open |
| The local-origin quantum for a geographic CRS in degrees | 4 | open |

## Notes

A task moves to `implemented` only when it has a test. A capability that exists
in a library but that no argument reaches is `implemented, not connected`, and
the capability matrix says so rather than claiming support.
