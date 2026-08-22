# Tiling and LOD Contract

Two separate concepts, deliberately not collapsed into one:

```text
tiling  -> spatial partitioning: which part of the raster a prim covers
LOD     -> sampling density:     how finely that part is represented
```

A tile can exist at several densities; a density can apply with or without
tiling. Merging them produces a design where you cannot change one without
changing the other.

## 1. Why tiling

A 40000 x 40000 float32 DEM at full density is 1.6 billion vertices. There is
no useful single `UsdGeomMesh` there. The stage, the memory, and Hydra all
concentrate the cost in one prim that cannot be partially loaded.

Tiling makes the unit of loading smaller than the dataset:

```text
/Terrain
    /L0
        /tile_0_0      payload -> tiles/L0/0_0.usdc
        /tile_0_1      payload -> tiles/L0/0_1.usdc
    /L1
        ...
```

Each tile is payload-backed, so composition can defer the geometry until the
consumer asks for it.

## 2. Four distinct tile concepts

Confusing these is the most common raster-tiling bug, so they have separate
names in the code:

| Concept | Meaning | Owner |
| --- | --- | --- |
| `SourceTile` | A native tile or strip in the source file | `usdGeoTiff` |
| `RasterWindow` | An arbitrary rectangle a caller wants, in full-resolution pixel coordinates | `usdRasterCore` |
| `SpatialTile` | A unit of USD spatial partitioning | `usdRasterTiling` |
| `PayloadTile` | An authored `.usdc` file backing one `SpatialTile` | `usdRasterAuthoring` |

A `SpatialTile` is **not required to match** a `SourceTile`. Source tiling is
chosen by whoever wrote the file, often 256 x 256; USD spatial tiling is chosen
for scene-graph and memory reasons, often much larger. The mapping between them
is planned, and the plan reports how many source tiles each spatial tile costs.

## 3. Tile identity

Tile identity is deterministic and derived from the source grid, never from
iteration order or from a hash of contents:

```text
tileId = (level, tileX, tileY)
```

with `tileX` and `tileY` in tile units at that level, and `(0, 0)` anchored at
source pixel `(0, 0)`. Anchoring at the source origin — not at the extent
centre, and not at a CRS-derived grid — keeps identity stable when a conversion
is rerun with a different window or a different level set.

Payload file names derive from the identity:

```text
tiles/L<level>/<tileX>_<tileY>.usdc
```

Two conversions of the same source with the same arguments produce byte-stable
identical names and contents. That is what makes the generated cache reusable
and the converter resumable.

## 4. Tile boundaries and seams

Adjacent tiles must not leave a visible crack. The rule:

**A tile includes one extra row and column of vertices on its high edges,
shared with the neighbouring tile.**

```text
tileSize = 512 samples
mesh per tile = 513 x 513 vertices, 512 x 512 quads
```

The duplicated boundary vertices are exactly coincident because they are
sampled from the same source pixels through the same transform with the same
local origin — not recomputed per tile. Using one shared local origin for the
whole conversion is what makes that exactness reliable; see
[COORDINATE_MODEL.md](COORDINATE_MODEL.md).

Tiles on the right and bottom margins are truncated to the remaining source
extent rather than padded, and their vertex counts are reported in the
manifest.

## 5. LOD profiles

The first stage maps profiles onto a sampling step. The values below are the
initial mapping, not a contract:

| Profile | Sampling step | Intent |
| --- | --- | --- |
| `off` | 1 | full source density, no decimation |
| `quality` | 1 | full density, with tiling and budgets applied |
| `balanced` | 4 | default for production preview |
| `preview` | 16 | interactive first look |

**The step values are not part of the public contract.** They live in one
profile table in `usdRasterTiling` and may be retuned. The profile name is the
stable interface. This is why `lod` is published before `meshStep`; see
[FILE_FORMAT_ARGUMENTS.md](FILE_FORMAT_ARGUMENTS.md).

Sampling never re-anchors the grid: sample `(0, 0)` is always source pixel
`(0, 0)` at every step, so levels stay registered with each other.

## 6. Decimation, not averaging, initially

The first implementation decimates: it takes every `s`-th sample. It does not
average, because averaging elevation across a NoData boundary produces values
that exist nowhere in the source, and because decimation is exactly
reproducible.

Averaged or minimum/maximum-preserving downsampling is a later, explicit
option. When it arrives it is recorded as a lossy transformation in authored
metadata, and it is never the silent default.

Where the source carries its own overviews — a COG pyramid — those are
preferred over decimating full-resolution data, because they are cheaper to
read and were generated with knowledge of the data. The reader reports which
level actually served a request.

## 7. Level construction

```text
L0   full density for the selected LOD profile
L1   half density, half as many tiles per axis
L2   quarter density
...
```

Levels stop when a level fits in a single tile. The level count is derived, not
configured, so a conversion cannot produce a truncated pyramid by accident.

Whether a consumer selects among levels is a consumer decision. This repository
authors the hierarchy; it does not implement selection, and no camera,
viewport, screen-space metric, or Hydra type appears in the tiling module.

## 8. Bounded memory

The converter processes one spatial tile at a time:

```text
for each spatial tile:
    plan the source windows it needs
    read them within the memory budget
    author the payload .usdc
    release the buffers
```

Peak memory is therefore a function of `tileSize` and the sample type, not of
source size. A conversion of a 20 GB DEM at `tileSize=512`, `float32` holds
roughly `513 * 513 * 4` bytes of samples plus the authored point array per
tile — about 5 MB — not 20 GB.

The manifest records peak memory and per-tile timings so that a regression is
measurable.

## 9. Manifest

A tiled conversion writes a manifest describing what it produced:

```text
source identity
normalized arguments
local origin
level count and per-level tile counts
per-tile: identity, source window, vertex count, payload path, byte size
sampling profile and applied step
NoData policy
I/O counters: requested bytes, fetched bytes, amplification
peak memory
plugin version
```

The manifest is what makes a conversion resumable and auditable, and it is the
input to the generated-cache key. See [CACHE.md](CACHE.md).

## 10. Root layer

```usda
def Xform "Terrain" (
)
{
    def Scope "L0"
    {
        def Xform "tile_0_0" (
            prepend payload = @tiles/L0/0_0.usdc@
        )
        {
        }
    }
}
```

The root layer carries the geospatial metadata for the whole dataset; each
payload carries the metadata specific to its tile, including its source window.
A tile is therefore interpretable on its own, which matters when a consumer
loads a subset.

## 11. Non-responsibilities

The tiling module does not:

- parse any format;
- include any OpenUSD header;
- decide when a consumer should load a tile;
- implement view-dependent selection;
- perform reprojection.
