# Raster Metadata Contract

This document fixes the metadata authored onto a raster prim: property names,
types, and meaning. It is normative for every representation and every bundle.

Names are namespaced `geo:` for values shared with `usd-pointcloud-plugins`, and
`raster:` for values specific to this data model. Keeping the shared prefix
identical across the two repositories is deliberate: a consumer that reads
`geo:crs` should not need to know whether the prim came from a point cloud or a
DEM.

## 1. Stage and prim shape

The stage is Y-up with one meter per unit.

```text
representation=metadata     /Raster     Scope
representation=mesh         /Raster     UsdGeomMesh
representation=image        /Raster     Xform
                            /Raster/Plane   UsdGeomMesh
```

A tiled conversion authors the same metadata on the root and, per tile, the
tile-specific subset marked below.

## 2. Shared geospatial metadata

Authored on every representation.

| Property | Type | Meaning |
| --- | --- | --- |
| `geo:crs` | `string` | The source CRS as `EPSG:<code>` when a code is available, otherwise WKT |
| `geo:crsWkt` | `string` | The full WKT, when the source carries one |
| `geo:linearUnit` | `string` | Horizontal unit of the source CRS, for example `metre` |
| `geo:verticalUnit` | `string` | Vertical unit, when the source declares one |
| `geo:localOrigin` | `double3` | Source coordinate corresponding to stage-local `(0, 0, 0)` |
| `geo:boundsMin` | `double3` | Minimum source-coordinate corner of the extent |
| `geo:boundsMax` | `double3` | Maximum source-coordinate corner of the extent |
| `geo:sourceIdentifier` | `string` | The resolved identifier the layer was authored from |

`geo:localOrigin` is the recovery key for the whole pipeline:

```text
sourceCoordinate = geo:localOrigin + stageLocalPosition
```

It is authored on every prim that carries stage-local positions, including
every tile payload, and it holds the same value across an entire conversion.

## 3. Raster description

Authored on every representation.

| Property | Type | Meaning |
| --- | --- | --- |
| `raster:width` | `uint64` | Source width in pixels, at full resolution |
| `raster:height` | `uint64` | Source height in pixels, at full resolution |
| `raster:bandCount` | `uint` | Number of bands in the source |
| `raster:geoTransform` | `double[]` | Six affine coefficients, ordered `a0, a1, a2, b0, b1, b2` |
| `raster:pixelAnchor` | `token` | `area` or `point` |
| `raster:pixelAnchorSource` | `token` | `file` or `argument`, recording whether it was read or supplied |
| `raster:dataType` | `token` | Source sample type of the selected band |
| `raster:noDataValue` | `double` | Source NoData value, when declared |
| `raster:nativeTileWidth` | `uint` | Native tile width, when the source is tiled |
| `raster:nativeTileHeight` | `uint` | Native tile height, when the source is tiled |
| `raster:overviewCount` | `uint` | Number of overview levels present in the source |

The geotransform coefficient order is fixed here and must not be reordered to
match any external tool convention. A consumer reading `raster:geoTransform`
follows this document, not GDAL habit.

## 4. Band description

Per-band values are authored as parallel arrays so that a consumer can read
band `n` without traversing children.

| Property | Type | Meaning |
| --- | --- | --- |
| `raster:bandIndices` | `uint[]` | 1-based band indices present in the source |
| `raster:bandDataTypes` | `token[]` | Sample type per band |
| `raster:bandDescriptions` | `string[]` | Description per band, when declared |
| `raster:bandUnits` | `string[]` | Unit per band, when declared |
| `raster:bandScales` | `double[]` | Scale per band, when declared |
| `raster:bandOffsets` | `double[]` | Offset per band, when declared |
| `raster:bandNoDataValues` | `double[]` | NoData per band, when declared |
| `raster:selectedBand` | `uint` | The band this prim was authored from |

The per-band arrays are parallel to `raster:bandIndices`. When a source has a
mixed set of declared and undeclared values, the corresponding array is omitted
rather than using a sentinel or shifting values between bands.

## 5. Conversion record

Authored whenever pixels were read. This is the record of what was done to the
data, and it is what makes a lossy step auditable.

| Property | Type | Meaning |
| --- | --- | --- |
| `raster:representation` | `token` | `metadata`, `mesh`, or `image` |
| `raster:samplingStep` | `uint` | Decimation step actually applied |
| `raster:lodProfile` | `token` | Profile name requested, when one was |
| `raster:overviewLevel` | `uint` | Source overview level that served the read |
| `raster:heightScale` | `double` | Vertical exaggeration applied |
| `raster:heightOffset` | `double` | Vertical offset applied |
| `raster:noDataPolicy` | `token` | `skip`, `fill`, or `keep` |
| `raster:fillValue` | `double` | Fill value, when `noDataPolicy` is `fill` |
| `raster:lossy` | `bool` | True when any lossy step was applied |
| `raster:lossyReasons` | `token[]` | For example `verticalExaggeration`, `narrowingConversion`, `noDataSubstitution`, `decimation` |
| `raster:pluginVersion` | `string` | Version of the bundle that authored the layer |

`raster:lossy` exists so a consumer can decide in one read whether the geometry
is a faithful representation of the source. `raster:lossyReasons` says why.

## 6. Tile metadata

Authored on each tile payload, in addition to the shared and description
values.

| Property | Type | Meaning |
| --- | --- | --- |
| `raster:tileLevel` | `uint` | Level index, `0` is the finest authored level |
| `raster:tileX` | `uint` | Tile column at that level |
| `raster:tileY` | `uint` | Tile row at that level |
| `raster:sourceWindow` | `uint64[]` | The source window this tile covers, ordered `x, y, width, height` |

Tile identity is anchored at source pixel `(0, 0)`; see
[TILING.md](../architecture/TILING.md).

## 7. Mesh-specific

Authored on `UsdGeomMesh` prims in addition to the above.

| Property | Value |
| --- | --- |
| `orientation` | `rightHanded`, authored explicitly |
| `subdivisionScheme` | `none`, authored explicitly |
| `extent` | Computed from the authored points |

Normals, UVs, and materials are not authored by the mesh representation. See
[REPRESENTATIONS.md](../architecture/REPRESENTATIONS.md).

## 8. Compatibility policy

1. A published property name and type are stable. A meaning change requires a
   new name.
2. A property absent from the source is absent from the stage. It is never
   authored with a sentinel value.
3. Additive changes — a new property — are minor-version changes. Removals and
   renames are recorded in a migration note.
4. When this document and an implementation disagree, the implementation is the
   bug.
