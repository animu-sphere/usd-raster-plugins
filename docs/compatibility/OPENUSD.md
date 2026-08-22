# OpenUSD Compatibility

## Declared contract

| Item | Value |
| --- | --- |
| OpenUSD version | 26.08 |
| Manifest declaration | `>=26.08,<27.0` |
| OpenStrata target | `cy2026` |
| OpenStrata profile | `usd` |
| Stage up axis | `Y` |
| Stage meters per unit | `1.0` |

This matches `usd-pointcloud-plugins`, so a host can load both plugin families
against one runtime.

## Planned platform coverage

| Host | Target | Status |
| --- | --- | --- |
| Windows 2022 x86_64 | cy2026 / usd | planned |
| Ubuntu 24.04 x86_64 | cy2026 / usd | planned |
| macOS 15 arm64 | cy2026 / usd | planned |

Plus a core-only lane on all three that builds and tests without OpenUSD.

## OpenUSD surface used

Deliberately narrow. Every additional API is a compatibility obligation across
future OpenUSD versions.

| Area | Used for |
| --- | --- |
| `SdfFileFormat` | Plugin registration and layer reading |
| `SdfLayer` | Authoring the generated layer |
| `SdfPath`, `SdfValueTypeNames` | Prim paths and attribute types |
| `ArResolver`, `ArAsset` | Opening the source through the active resolver |
| `UsdGeomMesh` | The mesh representation |
| `UsdGeomXform`, `UsdGeomScope` | Hierarchy and metadata-only prims |
| `UsdStage` metadata | Up axis and meters per unit |
| `TfDiagnostic` (`TF_RUNTIME_ERROR`, `TF_WARN`) | Diagnostic projection |
| `UsdShade` / `UsdPreviewSurface` | Only when the image representation lands |

Not used, and not planned:

- Hydra, `HdSceneIndex`, render delegates.
- `UsdLux`, `UsdPhysics`, `UsdSkel`.
- Custom schema generation (`usdGenSchema`). See
  [ADR-0001](../adr/0001-existing-usd-schemas.md).
- `UsdVolume` and field types.

### On `usdLod`

`usd-pointcloud-plugins` authors its LOD hierarchy through the OpenUSD 26.08
`usdLod` schemas. This repository does **not** commit to that for raster, and
the reason is worth recording: point-cloud LOD is a density choice over one
spatial region, while raster tiling is a spatial partition whose levels are a
pyramid. Whether `usdLod` is the right expression for a raster pyramid is an
open question that Milestone 6 answers with a real hierarchy, not in advance.

Until then, the tiling milestone authors payload-backed `Xform` hierarchies,
which need no schema beyond `UsdGeomXform`, and the question is revisited with
a working pyramid in hand.

## Dynamic FileFormat surface

The bundle registers as a dynamic file format so that `representation` and
`lod` can be exposed as prim metadata and so that changing them recomposes the
layer. The dynamic surface is kept to those two fields; every other argument
stays in `SDF_FORMAT_ARGS`.

Keeping it narrow is deliberate: each dynamic field is a compatibility surface
that a host may depend on, and a field added later is far cheaper than a field
removed later.

## Host expectations

A host loading this plugin must:

- provide an OpenUSD 26.08 runtime with a compatible ABI;
- register the bundle through `PXR_PLUGINPATH_NAME`;
- supply an `ArResolver` capable of resolving the assets it references —
  the default resolver suffices for local files;
- tolerate a `Read` that fails with a diagnostic for oversized requests, rather
  than expecting a degraded result.

The last point is the one that differs from a typical image plugin. A raster
source can be arbitrarily large, and this plugin refuses rather than silently
downgrades. See [ADR-0008](../adr/0008-preview-vs-converter.md).

## Version policy

- The declared range is upper-bounded at the next major version, and moving it
  requires running the full matrix against the new runtime.
- An OpenUSD API newly used is recorded in the table above in the same pull
  request.
- A behavior difference between OpenUSD versions is recorded in a migration
  note rather than absorbed silently into the code.
