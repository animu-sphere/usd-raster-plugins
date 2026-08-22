# Milestone 1: Raster Core

## Objective

Establish the format-independent raster value model and the byte-source
contract, with no OpenUSD and no TIFF knowledge, so that every later module
adapts to a settled shape rather than negotiating one.

## Scope

`libs/usd-geo-core` and `libs/usd-raster-core`.

### `usdGeoCore`

- `CrsDescription` — EPSG code, WKT, linear unit, vertical unit, all optional
  and all preserved as read.
- `GeoBounds` — `double` min/max in source coordinates.
- `LocalOrigin` and the source-to-stage-local transform.
- `TileId` — deterministic identity, anchored at source pixel `(0, 0)`.
- Cache-key input normalization.
- `Diagnostic`, `DiagnosticCode`, `Severity`, and the raster anchors defined in
  [DIAGNOSTICS.md](../architecture/DIAGNOSTICS.md).

### `usdRasterCore`

- `RasterSize`, `RasterWindow`, `RasterDataType`, `RasterBandInfo`.
- `RasterGeoTransform` with forward and inverse application, `PixelAnchor`, and
  the pixel-to-source conversion fixed in
  [COORDINATE_MODEL.md](../architecture/COORDINATE_MODEL.md).
- `RasterMetadata`, `RasterGrid`, `RasterTile`, `RasterSample`.
- `RasterReadOptions` including band, overview level, sampling step, output
  type, memory budget, and cancellation.
- NoData representation and comparison, including NaN handling.
- `RandomAccessSource` and `ReadResult`, plus `MemorySource` and
  `LocalFileSource`.

## Deliverables

- The value types above, with tests.
- Window arithmetic: intersection, clipping to raster bounds, subdivision into
  a tile grid, and conversion between full-resolution and overview coordinates.
- Affine transform tests covering north-up, south-up, rotated, and
  non-invertible cases.
- `MemorySource` and a source test double that records every requested byte
  range, for later I/O-selectivity assertions.
- Round-trip precision tests: a source coordinate near `1e6` survives the
  transform to stage-local `float` and back within the documented tolerance.

## Exit criteria

- The libraries build and all tests pass with no OpenUSD available.
- No header in either library includes an OpenUSD, libtiff, or transport
  header.
- Pixel-to-source conversion is correct for both anchoring conventions under
  north-up, south-up, and rotated transforms, verified against hand-computed
  values in the test.
- A window can be subdivided into tiles and reassembled with no gap and no
  overlap, for sizes that do not divide evenly.
- The instrumented source proves that a bounded read requests only the ranges
  it needs.

## No OpenUSD

This is the milestone where the OpenUSD-free property is established rather
than claimed. The core lane in CI builds these libraries with no OpenUSD
present, and that lane is required.

The value of it is concrete: transform bugs, window arithmetic bugs, and NoData
comparison bugs are found in a test that runs in milliseconds, not in a stage
that has to be opened.

## Design notes

**Windows are always full-resolution coordinates.** A caller asking for pixels
`(1000, 1000)`-`(1512, 1512)` gets the same region whether an overview serves
it or not. Putting overview rescaling in the caller would spread the same
arithmetic across every consumer.

**NoData comparison is exact.** Bit equality for NaN, value equality otherwise.
A tolerance would silently delete legitimate data at the tolerance boundary.

**Diagnostics carry raster anchors.** A pixel coordinate or a window in the
diagnostic is what makes a failure on a 40000-pixel-wide source actionable.

## Outstanding

- Whether `RasterGrid` should own its buffer or reference a caller-provided
  one. Owning is the default; a non-owning view is added only if a measured
  copy cost justifies it.
- Whether sub-byte sample formats (1, 2, 4 bits) enter `RasterDataType` now or
  when a format needs them. Deferred, because an unused enum value is a
  contract that nothing tests.
