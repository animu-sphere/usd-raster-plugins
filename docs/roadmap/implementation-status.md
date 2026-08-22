# Implementation Status

The task-level record of what exists. The
[capability matrix](../reference/CAPABILITY_MATRIX.md) records source support;
this file records work.

**Current state: nothing is implemented.** The repository holds documentation
and contracts. No library, plugin, or tool source exists, and no release has
been tagged.

Status words, from
[MODULE_README_CONTRACT.md](../contributing/MODULE_README_CONTRACT.md):

```text
implemented                   in this module, tested
implemented, not connected    exists in a library, no argument reaches it
planned                       has a contract, no implementation
not planned                   explicitly out of scope
```

## Milestone 0 — repository skeleton (in progress)

| Task | Status |
| --- | --- |
| Documentation taxonomy under `docs/` | done |
| Architecture contracts: workspace, coordinates, reader, representations, arguments, adapter, diagnostics, tiling, cache, resolver | done |
| ADR-0001 through ADR-0008 | done |
| Roadmap and milestone breakdown | done |
| Module README contract | done |
| Root `CMakeLists.txt` and core-only lane | not started |
| `openstrata.toml`, `openstrata.scaffold.yaml`, `openstrata.ci.yaml` | not started |
| `VERSION`, `CHANGELOG.md`, root `README.md` | not started |
| `LICENSE`, `NOTICE`, `THIRD_PARTY_NOTICES.md` | not started |
| Generated GitHub workflow | not started |
| Fixture-generation script | not started |

Exit criteria are in
[phase-0-repository-skeleton.md](phase-0-repository-skeleton.md).

## Milestone 1 — raster core (planned)

| Task | Status |
| --- | --- |
| `usdGeoCore` value types and diagnostics | planned |
| `usdRasterCore` raster value model | planned |
| Affine transform, forward and inverse | planned |
| Window arithmetic and tile subdivision | planned |
| NoData representation and comparison | planned |
| `RandomAccessSource`, `MemorySource`, `LocalFileSource` | planned |
| Instrumented source test double | planned |
| Precision round-trip tests | planned |

Detail in [phase-1-raster-core.md](phase-1-raster-core.md).

## Milestone 2 — GeoTIFF metadata (planned, `v0.1.0`)

| Task | Status |
| --- | --- |
| TIFF header, both endiannesses, classic and BigTIFF | planned |
| IFD traversal and tag validation | planned |
| Strip and tile layout description | planned |
| Overview discovery | planned |
| GeoTIFF key decoding | planned |
| `ModelPixelScale` / `ModelTiepoint` / `ModelTransformation` | planned |
| NoData and band metadata | planned |
| `representation=metadata` authoring | planned |
| Plugin registration and discovery test | planned |
| `GTIF001`-`GTIF009` with a fixture each | planned |
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

| Question | Owner milestone |
| --- | --- |
| libgeotiff versus a minimal in-repository GeoTIFF key decoder | 2 |
| Whether PROJ is introduced, and where its boundary sits | after 4 |
| The interactive vertex ceiling value | 4 |
| The `lod` profile step values | 5 |
| Whether `RasterGrid` owns or views its buffer | 1 |
| Whether sub-byte sample formats enter `RasterDataType` | deferred |
| How a geographic-CRS source is handled in a metric stage | 4 |
| Whether ASan and TSan run per pull request or nightly | 0 |

## Notes

A task moves to `implemented` only when it has a test. A capability that exists
in a library but that no argument reaches is `implemented, not connected`, and
the capability matrix says so rather than claiming support.
