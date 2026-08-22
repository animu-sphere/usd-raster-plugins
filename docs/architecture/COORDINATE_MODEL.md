# Raster Coordinate Contract

Coordinates are the part of a raster pipeline that breaks most often, and the
breakage is usually silent: a half-pixel shift, a flipped row order, or a
metre-versus-degree unit confusion produces a stage that looks plausible and is
wrong. This document fixes the conversion from source pixels to stage-local
USD coordinates so that the conversion is testable rather than folklore.

Every statement here is normative and is covered by golden tests. See
[ADR-0006](../adr/0006-local-origin-coordinate-policy.md).

## 1. The chain

```text
pixel index (i, j)
      |  pixel anchoring
      v
pixel coordinate (px, py)          continuous, in pixel space
      |  affine geotransform
      v
source coordinate (X, Y)           in the source CRS, double
      |  band value + vertical unit
      v
source coordinate (X, Y, Z)        double
      |  subtract localOrigin
      v
stage-local coordinate             float, USD
      |  up-axis convention
      v
UsdGeomMesh point
```

No stage of this chain is implicit. Each conversion is a named function in
`usdGeoCore` or `usdRasterCore`, and each is independently tested.

## 2. Pixel indices and anchoring

`i` is the column index, `0 <= i < width`. `j` is the row index,
`0 <= j < height`. `j = 0` is the first row stored in the file, which for a
conventional north-up raster is the northernmost row.

GeoTIFF distinguishes two anchoring conventions through
`GTRasterTypeGeoKey`:

| Convention | Meaning | Pixel coordinate of sample `(i, j)` |
| --- | --- | --- |
| `pixel-is-area` (`RasterPixelIsArea`) | A pixel is a cell covering an area; the geotransform maps the *upper-left corner* of pixel `(0, 0)` | `px = i + 0.5`, `py = j + 0.5` |
| `pixel-is-point` (`RasterPixelIsPoint`) | A pixel is a point sample; the geotransform maps the *centre* of pixel `(0, 0)` | `px = i`, `py = j` |

The reader reports the anchoring it found. It is never guessed. When
`GTRasterTypeGeoKey` is absent, the reader reports the anchoring as unknown and
the plugin either applies an explicit `pixelAnchor` argument or fails with a
diagnostic; it does not default silently.

**The sampled position for a mesh vertex is always the pixel centre.** The
distinction above is only about how the file relates its geotransform to the
grid, not about which position a vertex takes.

Consequence for bounds. Under `pixel-is-area`, the geographic extent of the
raster is the union of cell areas, so the extent spans `width` cells. Under
`pixel-is-point`, the extent spans the outermost sample positions, which is
`width - 1` intervals. Reported `geo:bounds` follows the convention actually in
force and records which one it was.

## 3. Affine geotransform

The geotransform is the six-coefficient affine map

```text
X = a0 + a1 * px + a2 * py
Y = b0 + b1 * px + b2 * py
```

stored as `RasterGeoTransform`:

```cpp
struct RasterGeoTransform {
    double a0, a1, a2;   // X = a0 + a1*px + a2*py
    double b0, b1, b2;   // Y = b0 + b1*px + b2*py
};
```

For a north-up raster, `a2` and `b1` are zero, `a1` is the positive pixel width
and `b2` is the *negative* pixel height, because row index increases southward
while `Y` increases northward. A positive `b2` is legal and means a
south-up raster; it is handled, not rejected, and it is recorded in authored
metadata.

`a2` or `b1` non-zero means a rotated raster. Rotated transforms are supported
by the transform code from the start, because a rotation that is ignored
produces silently misplaced geometry. Whether a given authoring path accepts a
rotated source is a separate question recorded in the capability matrix.

GeoTIFF supplies the coefficients through one of three tag sets, in this
precedence order:

1. `ModelTransformationTag` (34264) — a full 4x4 matrix; the affine terms are
   taken directly.
2. `ModelTiepointTag` (33922) + `ModelPixelScaleTag` (33550) — the common
   north-up case.
3. Neither — the raster is ungeoreferenced. The reader reports this; the plugin
   requires explicit arguments or fails.

When both (1) and (2) are present and disagree beyond a tolerance, the reader
emits a conflict diagnostic and prefers `ModelTransformationTag`.

The inverse transform is also part of the contract: window planning and tile
mapping need world-to-pixel, and it is computed by inverting the affine map
rather than by assuming a north-up special case.

## 4. Elevation and the vertical axis

For an elevation representation, the band value is the `Z` coordinate:

```text
Z = heightScale * bandValue + heightOffset
```

`heightScale` and `heightOffset` default to `1.0` and `0.0`. Where the source
records a scale/offset for the band, that is applied first, and `heightScale`
is applied on top of it as a display-side exaggeration. Vertical exaggeration
is a lossy display choice: when it is not `1.0`, it is recorded in authored
metadata so the original elevation is recoverable.

The vertical CRS, vertical unit, and geoid/ellipsoid reference are read where
available and preserved as metadata. This repository does not perform vertical
datum transformation. A source whose vertical unit differs from its horizontal
unit is not silently mixed; the unit is recorded and, when a conversion is
requested, it is explicit.

## 5. Local origin

Projected coordinates are routinely in the hundreds of thousands or millions of
metres. `float` has roughly 7 decimal digits of precision, so a coordinate near
`500000.0` resolves to about 0.03 m — visible wobble on a terrain surface.
Geographic coordinates in degrees are worse in relative terms.

Therefore:

- All internal computation is `double`.
- Authored `points` are `float`, expressed relative to an explicit local
  origin.
- The origin is authored as `geo:localOrigin` (a `double3`) so the source
  coordinate is exactly recoverable:

```text
sourceCoordinate = geo:localOrigin + stageLocalPosition
```

The default local origin is the centre of the raster extent, rounded to a
stable value so that adding a tile or changing a window does not move it. For
a tiled conversion, **one local origin is shared by every tile in the
conversion**; per-tile origins are not used, because they make tiles
non-composable and complicate the recovery formula.

The origin is part of the conversion manifest and of the cache key. It is
recorded once and reused, never recomputed per read.

## 6. Up axis and handedness

The authored stage is Y-up with one meter per unit, matching the OpenUSD
default and `usd-pointcloud-plugins`.

The mapping from a projected CRS, which is X-east / Y-north / Z-up, onto a
Y-up stage is:

```text
stage.x =  source.X - origin.X
stage.y =  source.Z - origin.Z        (elevation)
stage.z = -(source.Y - origin.Y)      (north becomes -Z)
```

The negation on the third component preserves a right-handed coordinate system
and keeps north pointing away from the viewer in a default camera. It is
applied in exactly one place, in `usdRasterAuthoring`, and is covered by a
golden test that pins the sign.

Mesh face winding is counter-clockwise when viewed from the direction the
normal faces, and `orientation` is authored explicitly as `rightHanded`. A
grid whose geotransform has negative determinant would otherwise produce
inverted winding; the authoring code compensates from the transform rather
than from a hard-coded assumption.

## 7. Units

`geo:linearUnit` records the horizontal unit of the source CRS, and
`geo:verticalUnit` the vertical unit. A stage unit is metres.

A geographic CRS (degrees) has no linear unit and cannot be authored directly
into a metric stage without a projection choice. Until a projection step
exists, a geographic source is either:

- authored with an explicit `degreeScale` argument that the caller supplies
  and that is recorded as a lossy, non-geodetic approximation; or
- rejected with a diagnostic naming the missing projection.

It is never authored as if degrees were metres without a record of it.

## 8. Grid to mesh

For an `N x M` sampled grid with sampling step `s`, the authored mesh has

```text
vertices = ceil(N / s) * ceil(M / s)
quads    = (ceil(N / s) - 1) * (ceil(M / s) - 1)
```

Vertex order is row-major starting at `(i = 0, j = 0)`. Face vertex indices for
the quad whose upper-left vertex is `(u, v)` in sampled-grid coordinates are

```text
(v * W + u), (v * W + u + 1), ((v + 1) * W + u + 1), ((v + 1) * W + u)
```

where `W` is the sampled width. This order is fixed so that golden tests can
compare topology byte-for-byte.

A sampling step never re-anchors the grid: sample `(0, 0)` is always source
pixel `(0, 0)`, so the north-west corner is stable across LOD levels and a
lower-resolution mesh remains registered with a higher-resolution one.

## 9. NoData

A cell whose value equals the source NoData value carries no elevation. The
policy is explicit, selected by argument, and recorded in metadata:

| Policy | Behavior |
| --- | --- |
| `skip` (default) | Any quad with at least one NoData corner produces no face. Vertices may remain unreferenced or be dropped, but the choice is fixed and tested. |
| `fill` | NoData cells take an explicit fill value supplied by argument. |
| `keep` | The raw NoData value is written as an elevation. Only useful for inspection; recorded as lossy. |

Floating-point NoData comparison uses exact bit equality for NaN-valued NoData
and exact value equality otherwise. A tolerance is not applied, because a
tolerance silently removes legitimate data.

## 10. Golden tests

The following fixtures pin this contract. Each is a small synthetic GeoTIFF
generated by a checked-in script, with a recorded expected mesh or metadata
result:

```text
1x1                      degenerate: no faces, valid metadata
2x2                      the minimal single-quad case
3x3                      odd size, centre sample
5x3                      non-square, exercises row/column confusion
pixel-is-area            anchoring and bounds
pixel-is-point           anchoring and bounds
negative-y-scale         conventional north-up
positive-y-scale         south-up
rotated-transform        non-zero a2 / b1
model-transformation     tag 34264 path
tiepoint-and-scale       tag 33922 + 33550 path
conflicting-transform    both tag sets, disagreeing
no-georeference          absent transform tags
uint8 / uint16 / float32 sample formats
nodata-skip / -fill      NoData policies
nan-nodata               NaN NoData handling
large-offset             coordinates near 1e6, precision check
geographic-degrees       geographic CRS rejection or explicit scale
```

A change that alters any authored coordinate must update a golden file, and a
golden update is reviewed as a contract change.

## 11. Non-responsibilities

This contract does not cover, and this repository does not perform:

- Reprojection between coordinate reference systems.
- Vertical datum or geoid transformation.
- Geodetic distance or area computation.
- Time or epoch handling for dynamic reference frames.

CRS values are read, preserved, and reported. Interpretation beyond the affine
mapping described here belongs to the caller or to a future explicit
projection step recorded as its own ADR.
