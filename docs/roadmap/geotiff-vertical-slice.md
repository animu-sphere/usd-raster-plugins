# GeoTIFF Vertical Slice

Milestones 2 through 5 in detail. The organizing rule is that one narrow path
runs end to end before anything widens.

```text
2x2 synthetic GeoTIFF
        |
        v
    usdGeoTiff            TIFF header, IFD, geo keys, one window
        |
        v
  RasterGrid<float>
        |
        v
 usdRasterAuthoring       geotransform, local origin, mesh topology
        |
        v
   UsdGeomMesh
        |
        v
raster-geotiff plugin     registration, arguments, diagnostics
        |
        v
     usdview
```

Eight things this slice validates, each of which every later feature depends
on: FileFormat registration, TIFF parsing, geotransform interpretation, mesh
topology, elevation values, bounds, CRS metadata, and the local origin.

Compression breadth, tiling, and remote access widen only after it passes.

---

## Milestone 2 — GeoTIFF metadata (`v0.1.0`)

### Scope

`libs/usd-geotiff` and a metadata-only `plugins/raster-geotiff`.

- TIFF header: both endiannesses, classic and BigTIFF.
- IFD traversal, tag reading, and offset validation.
- Dimensions, band count, sample format, bits per sample, planar
  configuration.
- Strip and tile layout description, without decoding.
- Overview (multi-IFD) discovery, recorded but not yet used.
- GeoTIFF keys: `GTModelTypeGeoKey`, `GTRasterTypeGeoKey`, projected and
  geographic CRS keys, linear and angular unit keys.
- `ModelPixelScaleTag`, `ModelTiepointTag`, `ModelTransformationTag`, with the
  precedence and conflict rules from
  [COORDINATE_MODEL.md](../architecture/COORDINATE_MODEL.md).
- `GDAL_NODATA` and per-band NoData.
- Band descriptions, units, scale, and offset where present.

### Deliverables

- `GeoTiffReader::ReadMetadata`, reading no pixel data.
- `representation=metadata` authoring: a `Scope` with the `geo:` and `raster:`
  properties fixed in
  [RASTER_METADATA.md](../reference/RASTER_METADATA.md).
- The `GTIF001`-`GTIF009` diagnostics with a fixture each.
- Golden metadata fixtures for the georeferencing cases listed in the
  coordinate contract.

### Exit criteria

- Opening a fixture produces correct metadata with a byte-count assertion
  proving only the header and IFDs were read.
- Every georeferencing tag combination in the coordinate contract produces the
  documented geotransform, or the documented diagnostic.
- Pixel anchoring is never guessed: an absent `GTRasterTypeGeoKey` produces a
  warning and requires the `pixelAnchor` argument.
- The plugin is discovered from an installed layout and opens `.tif` and
  `.tiff`.

### Open decision

Whether GeoTIFF key decoding uses libgeotiff or a minimal in-repository
decoder. Resolved in this milestone with an ADR, once the pinned runtime is
known to supply libgeotiff or not.

---

## Milestone 3 — Pixel reading (`v0.2.0`)

### Scope

Windowed decoding in `usdGeoTiff`.

- Strip-organized and tile-organized reads.
- Compression: none, Deflate, LZW, PackBits, with horizontal and
  floating-point predictors.
- Sample formats: `uint8`, `uint16`, `uint32`, `int8`, `int16`, `int32`,
  `float32`, `float64`.
- Chunky and separate planar configuration.
- Single-band selection.
- `ReadWindow`, `ReadTile`, `ReadScanlines`.
- Output type conversion, with band scale and offset applied through `double`.
- Read planning: window to intersecting strips or tiles, adjacent-range
  coalescing, and requested-versus-fetched byte counters.

### Exit criteria

- A window read from a tiled fixture touches only the intersecting tiles,
  proven by the instrumented source.
- A window read from a striped fixture reports the expected I/O amplification
  rather than hiding it.
- Every supported compression and sample format decodes to values matching a
  golden file, and unsupported ones produce `GTIF004` or `GTIF005`.
- Memory during a windowed read of a large fixture stays within the configured
  budget, measured rather than assumed.
- A cancellation callback stops a read at a tile boundary and releases buffers.

### Why this precedes mesh authoring

Building mesh authoring on a whole-image read would make every later milestone
inherit an unbounded memory model, and tiling would then be a rewrite rather
than an addition. See [ADR-0003](../adr/0003-windowed-raster-reader.md).

---

## Milestone 4 — `UsdGeomMesh` (`v0.3.0`)

### Scope

`libs/usd-raster-authoring`, the first OpenUSD-dependent module.

- Regular grid mesh from one scalar band.
- The full coordinate chain: pixel anchoring, geotransform, elevation scale and
  offset, local origin, axis mapping.
- Fixed vertex and face ordering.
- NoData policies: `skip`, `fill`, `keep`.
- `extent`, explicit `orientation` and `subdivisionScheme`.
- The `geo:` and `raster:` metadata, including the conversion record.
- The interactive vertex ceiling in the plugin, with `GTIF012`.

### Exit criteria

- The 2x2 fixture produces exactly one quad with hand-verified vertex
  positions.
- Every golden fixture in the coordinate contract produces byte-identical
  topology and positions across Windows, Linux, and macOS.
- `sourceCoordinate = geo:localOrigin + stageLocalPosition` recovers the source
  coordinate to the documented tolerance for a fixture with coordinates near
  `1e6`.
- A sampling step of 1, 2, and 4 over the same source produces meshes that stay
  registered — sample `(0, 0)` is the same world position at every step.
- A NoData fixture produces the documented face count under each policy, and
  the applied policy appears in metadata.
- A request above the vertex ceiling fails with an actionable message naming
  the converter.

### Measurement

The ceiling value is chosen here from measurement, not guessed: open time and
peak memory for meshes at 1 M, 4 M, and 16 M vertices on each platform, in
usdview and through a plain stage open.

---

## Milestone 5 — Dynamic FileFormat arguments (`v0.3.0`)

### Scope

The argument surface fixed in
[FILE_FORMAT_ARGUMENTS.md](../architecture/FILE_FORMAT_ARGUMENTS.md):
`representation`, `band`, `lod`, `heightScale`, `nodata`, `fillValue`, `epsg`,
`pixelAnchor`.

- Parsing and normalization in the plugin, validation in the shared layer.
- Dynamic file-format registration, with `representation` and `lod` as the
  narrow dynamic surface.
- Profile-to-sampling-step table in `usdRasterTiling`, in one place.

### Exit criteria

- Each argument, valid and invalid, produces the documented result or
  diagnostic.
- An unknown argument name fails; it is never ignored.
- Two normalizing-equal spellings share a layer; two normalizing-different
  values do not.
- An argument that alters topology never reuses a layer authored under a
  different value.
- Changing a dynamic field recomposes the layer.

### Note on profiles

`lod` is published before `meshStep` deliberately. The profile names are the
stable interface, and the step values behind them are retuned from measurement
without breaking existing layer identities. Publishing a raw step first would
freeze it as a compatibility surface.
