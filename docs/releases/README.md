# Release records

Each tagged version receives an immutable record here: what shipped, the
supported behavior, build requirements, known limitations, and licensing notes.
Release records are history and are not rewritten after publication.

Released versions are recorded below. The remaining planned sequence follows.

| Version | Date | Record |
| --- | --- | --- |
| v0.1.0 | 2026-08-27 | [v0.1.0.md](v0.1.0.md) — GeoTIFF metadata FileFormat and packaged acceptance |
| v0.2.0 | 2026-09-05 | [v0.2.0.md](v0.2.0.md) — GeoTIFF windowed raster reads |

| Version | Planned theme |
| --- | --- |
| v0.3.0 | DEM to `UsdGeomMesh`: height mesh, coordinate transform, local origin, format arguments |
| v0.4.0 | Bounded-memory tiling: tiled mesh authoring and payloads |
| v0.5.0 | Production converter: CLI, manifests, generated cache |
| v0.6.0 | Resolver-backed GeoTIFF: remote `ArAsset` reads and `usd-http-resolver` compatibility |
| v0.7.0 | COG-aware optimization: tile-aware planning, remote selectivity, performance baseline |

Scope per release is in the [roadmap](../roadmap/README.md).

Prepare the record in the release commit immediately before creating its tag.
The tag pins the source commit and the record pins the release scope; runtime
digests and published artifact checksums are appended to the generated release
notes by the release workflow.

Unreleased work on `main` is tracked in the root `CHANGELOG.md` and, at task
granularity, in
[roadmap/implementation-status.md](../roadmap/implementation-status.md).

## Release gate

A release record is created only after:

1. `VERSION`, `openstrata.toml`, the plugin manifest, the plugin CMake project,
   the tag, and the finalized changelog version agree;
2. every declared hosted CI cell in `openstrata.ci.yaml` passes, including the
   core-only lane;
3. package digests are reproducible for an unchanged build;
4. notices, SBOM, and target metadata are verified, including that no test-only
   dependency is present in the artifact, per
   [DISTRIBUTION.md](../guides/DISTRIBUTION.md);
5. the capability matrix and the implementation status match what actually
   shipped;
6. the release is assembled as a draft for human review.

Item 5 is specific to this repository: the capability matrix is the document a
user reads to decide whether their file will open, and a release that widens
support without updating it has shipped a documentation bug.

## Release procedure

Run from the repository root before creating the tag:

1. Set the release version in `VERSION`, `openstrata.toml`,
   `plugins/raster-geotiff/openstrata.plugin.yaml`, and
   `plugins/raster-geotiff/CMakeLists.txt`.
2. Run the release metadata check to verify every package version declaration
   matches `VERSION`.
3. Update `CHANGELOG.md`, the release record, and any capability or
   compatibility documentation that changed.
4. Run `ost ci validate`, `ost configure`, `ost build`, and `ost test`, plus the
   core-only CMake lane.
5. Package with `ost plugin package --workspace --product --target cy2026
   --profile usd --json`, then inspect the generated product, manifest, and
   SBOM names for the expected version.
6. Commit the complete release preparation change and create `vX.Y.Z` on that
   commit. Push the tag only after the checks pass.

The release workflow validates the tag against `VERSION` and checks out the
tagged commit on every platform. Changes made after tagging are not included in
that release build.

## Record template

```text
# vX.Y.Z

Released YYYY-MM-DD

## Scope
## Supported behavior
## Authored USD
## File-format arguments
## Known limitations
## Build requirements
## Dependencies and licensing
## Verification
```

The **Known limitations** section is required and is written honestly. For a
raster plugin, the limitations a user hits first — unsupported compression, a
vertex ceiling, a striped source that reads slowly over a network — are more
useful than the feature list.
