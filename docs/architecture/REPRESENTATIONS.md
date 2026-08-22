# Raster Representation Contract

One raster source can be authored into more than one USD shape. This document
fixes which shapes exist, what each one authors, and which one a given
`representation` argument selects.

The governing rule is [ADR-0001](../adr/0001-existing-usd-schemas.md): no
repository-specific schema is defined. Every representation is expressed with
schemas that already exist in OpenUSD.

## 1. The set

| `representation` | Authored shape | Reads pixels | Primary use |
| --- | --- | --- | --- |
| `metadata` | `Scope` with geospatial metadata | no | inspection, CRS/bounds/band checks, huge or remote sources |
| `mesh` | `UsdGeomMesh` | yes | DEM, DSM, terrain, elevation grids |
| `image` | `UsdGeomMesh` plane + `UsdPreviewSurface` / `UsdUVTexture` | metadata only, initially | orthophoto, classified raster overlay |

`heightfield` is deliberately **not** in this table. It is an internal raster
semantic that selects the `mesh` authoring path, not a separate output shape
and not a schema. See [ADR-0005](../adr/0005-raster-to-mesh.md).

The default representation is `metadata`. Opening a raster must never
accidentally trigger a multi-gigabyte decode; producing geometry is an explicit
request.

## 2. `metadata`

Authors the geospatial description with no pixel access at all.

```usda
def Scope "Raster" (
)
{
    custom uint     geo:width       = 4096
    custom uint     geo:height      = 4096
    custom uint     geo:bandCount   = 1
    custom string   geo:crs         = "EPSG:6677"
    custom double[] geo:geoTransform = [...]
    custom string   geo:pixelAnchor = "area"
    custom double   geo:noData      = -9999
    custom double3  geo:boundsMin   = (...)
    custom double3  geo:boundsMax   = (...)
    custom double3  geo:localOrigin = (...)
}
```

The exact property names and types are fixed in
[RASTER_METADATA.md](../reference/RASTER_METADATA.md).

This representation is what makes a remote 20 GB DEM inspectable over a few
kilobytes of Range requests: only the header and the geo keys are read.

## 3. `mesh`

Converts one scalar band into a regular grid mesh.

```text
NxM sampled grid
   -> N*M vertices
   -> (N-1)*(M-1) quads
```

Authored:

- `points` — `float[]`, stage-local, relative to `geo:localOrigin`
- `faceVertexCounts` / `faceVertexIndices` — the fixed quad ordering from
  [COORDINATE_MODEL.md](COORDINATE_MODEL.md)
- `extent`
- `orientation` — explicit `rightHanded`
- `subdivisionScheme` — explicit `none`; this is a sampled surface, not a
  subdivision cage
- the same `geo:` metadata as `metadata`, plus the sampling step, the height
  scale, and the NoData policy actually applied

Not authored in the first implementation: normals, UVs, and materials.
Normals are computable by the consumer from a regular grid, and authoring them
doubles the point payload; adding them later is an additive change, removing
them later is not.

Vertex count is the hard constraint. A 4096 x 4096 source at step 1 is 16.7 M
vertices, which is a 200 MB `points` array. That is why the mesh path has a
documented ceiling in the plugin and why the production path is tiling; see
[TILING.md](TILING.md) and [ADR-0008](../adr/0008-preview-vs-converter.md).

### NoData

Quads touching a NoData cell follow the selected policy — `skip`, `fill`, or
`keep` — as fixed in [COORDINATE_MODEL.md](COORDINATE_MODEL.md). The applied
policy is authored as metadata, because a mesh with holes and a mesh with a
fill value are not interchangeable downstream.

## 4. `image`

An RGB or RGBA raster is not naturally geometry. It is a texture with a
geographic footprint.

First implementation authors:

- a `UsdGeomMesh` plane covering the raster footprint in stage-local
  coordinates, with a `st` primvar,
- the source asset path,
- the UV mapping relationship between pixel space and the plane,
- the same `geo:` metadata as `metadata`.

Deferred to a later milestone: a full `UsdPreviewSurface` / `UsdUVTexture`
network, and any renderer-specific material graph. The reason for deferring is
that a texture reference is only useful if the consumer can actually resolve
and decode the raster as an image — which for a tiled float32 GeoTIFF is not
generally true. Authoring a broken material is worse than authoring a
documented footprint with a source reference.

An `image` representation of a non-RGB source is a diagnostic, not a
grayscale guess.

## 5. Choosing between them

```text
scalar band, elevation semantics      -> mesh
scalar band, inspection only          -> metadata
3 or 4 bands, 8-bit                   -> image
anything remote and large, first look -> metadata
```

The reader does not choose. The plugin adapter maps the normalized
`representation` argument onto one authoring entry point, and an unrecognized
or inapplicable value fails with a diagnostic rather than falling back.

## 6. Representation and identity

`representation` changes what the layer contains, so it is part of layer
identity and of the cache key. A layer authored under `representation=mesh` is
never reused for `representation=metadata`, and the two never share a cache
entry. See [FILE_FORMAT_ARGUMENTS.md](FILE_FORMAT_ARGUMENTS.md) and
[CACHE.md](CACHE.md).

## 7. Future representations

Candidates, none of them committed:

| Candidate | Trigger for reconsidering |
| --- | --- |
| Point sampling to `UsdGeomPoints` | A real workflow that wants raster cells as points; would reuse `usd-pointcloud-plugins` contracts rather than duplicating them |
| Volume / `OpenVDB` for multi-band stacks | NetCDF or GRIB support with a genuine 3D variable |
| Curve extraction (contours) | Never; that is analysis, not ingestion |

A new representation requires an ADR, because it expands the public contract
of every plugin that exposes the `representation` argument.
