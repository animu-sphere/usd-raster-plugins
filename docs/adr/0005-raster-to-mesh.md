# ADR-0005: Raster to Mesh Is the First Geometry Representation

## Status

Accepted for Milestone 0. Implemented from Milestone 4.

## Decision

The first geometric representation of a raster is a regular grid
`UsdGeomMesh` authored from one scalar band:

```text
NxM sampled grid -> N*M vertices -> (N-1)*(M-1) quads
```

Vertex order is row-major from sample `(0, 0)`; face vertex ordering is fixed
in [COORDINATE_MODEL.md](../architecture/COORDINATE_MODEL.md) so golden tests
can compare topology exactly.

NoData cells follow an explicit policy — `skip`, `fill`, or `keep` — never an
implicit one.

`heightfield` is an internal semantic that selects this path, not a separate
representation and not a schema.

## Rationale

- A mesh is the representation every USD consumer already understands. It opens
  in usdview and renders in Hydra with no additional plugin.
- A regular grid needs no triangulation decisions, no spatial index, and no
  adaptive refinement, so the first implementation validates the coordinate,
  NoData, and precision contracts rather than a meshing algorithm.
- Topology is a pure function of grid dimensions, which makes it exactly
  testable. A wrong coordinate transform shows up as a byte difference in a
  golden file, not as a visual judgement.
- Deferring normals, UVs, and materials keeps the first payload small and keeps
  additive changes possible later. Adding attributes later is compatible;
  removing them is not.

Alternatives considered:

- **Adaptive / TIN meshing.** Produces far fewer triangles for the same visual
  quality, and requires an error metric, a refinement strategy, and NoData
  boundary handling. All of that is worth doing later, on top of a correct
  regular-grid pipeline, not instead of one.
- **`UsdGeomPoints` per cell.** Loses the surface, and duplicates the data model
  `usd-pointcloud-plugins` already owns.
- **A volumetric representation.** Wrong shape for a 2.5D elevation grid.

## Consequences

- Vertex count is the binding constraint. A 4096 x 4096 source at full density
  is 16.7 M vertices and roughly a 200 MB `points` array, which is why the
  plugin enforces an interactive ceiling and the production path is tiling. See
  [ADR-0008](0008-preview-vs-converter.md) and
  [TILING.md](../architecture/TILING.md).
- Decimation, not averaging, is the first sampling strategy: it is exactly
  reproducible and it does not invent values across NoData boundaries.
- `skip` as the default NoData policy means a mesh may have holes, and the
  applied policy is authored as metadata so a consumer can tell a hole from
  missing geometry.
- Multi-band and RGB sources are not served by this representation. They wait
  for the image representation.
