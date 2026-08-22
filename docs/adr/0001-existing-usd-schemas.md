# ADR-0001: Existing USD Schemas Before Custom Schemas

## Status

Accepted for Milestone 0. Applies to every representation.

## Decision

Define no repository-specific USD schema. Author raster data through schemas
that already exist in OpenUSD:

```text
elevation grid   -> UsdGeomMesh
orthophoto       -> UsdGeomMesh plane + image asset
                    (UsdPreviewSurface / UsdUVTexture later)
description only -> Scope with namespaced metadata
```

Values without a standard representation are authored as namespaced metadata
(`geo:`, `raster:`) or as primvars, following
[RASTER_METADATA.md](../reference/RASTER_METADATA.md).

"Heightfield" is an internal raster semantic that selects the mesh authoring
path. It is not a schema and not an authored type.

## Rationale

A custom schema would be the most natural-looking design and the most
expensive one:

- Every consumer would need the schema library installed to interpret a stage.
  A `UsdGeomMesh` opens in usdview, in a DCC, and in any Hydra renderer with no
  extra dependency.
- Schema definitions are versioned artifacts with their own compatibility
  obligations. Adding one before the data model is understood commits the
  project to migrating it.
- The information a custom schema would carry — CRS, geotransform, NoData,
  band description — is metadata, not geometry. Namespaced metadata carries it
  without inventing a type.
- `usd-pointcloud-plugins` reached the same conclusion for point clouds and
  authors `UsdGeomPoints`; consistency across the two repositories is worth
  preserving.

## Consequences

- Interoperability is available on day one: any USD consumer can display an
  authored terrain.
- Metadata names become the compatibility surface, so they are fixed in a
  reference document and treated as published.
- Some raster semantics are representable only as metadata that a generic
  consumer will ignore. That is accepted; the information is preserved and
  recoverable rather than enforced.
- If OpenUSD or its ecosystem later standardizes a raster or heightfield
  schema, an adapter is added on top of the existing pipeline rather than a
  rewrite. The internal `RasterGrid` contract does not change.

## Revisiting

Reconsider only if a concrete workflow cannot be expressed through existing
schemas plus metadata, and the gap is demonstrated with a failing use case
rather than anticipated. A new representation requires its own ADR.
