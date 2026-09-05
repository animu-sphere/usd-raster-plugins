# usdRasterCore

## Purpose

The format-independent raster value model and the byte-source contract every
reader consumes. It settles the shape that `usdGeoTiff` and every later format
reader adapt to, rather than each reader negotiating its own — and it settles
the two things that break most often in a raster pipeline: the pixel-to-source
coordinate conversion, and window arithmetic.

**OpenUSD is not required.** This module builds and tests with no OpenUSD
runtime present. Transform bugs, window arithmetic bugs, and NoData comparison
bugs are found here in a test that runs in milliseconds, not in a stage that
has to be opened.

## Responsibilities

- `RasterSize`, `RasterDataType`, and the conversion-exactness rules.
- `RasterWindow` — window arithmetic: intersection, clipping, subdivision into
  a tile grid, and conversion to and from overview coordinates.
- `RasterGeoTransform` and `PixelAnchor` — the affine map, its inverse, and the
  pixel-to-source conversion under both anchoring conventions.
- `RasterBandInfo`, `RasterMetadata` — everything readable without decoding a
  pixel.
- `RasterGrid` — the decoded result of one window read.
- `NoDataValue`, `NoDataPolicy` — the NoData representation and its exact
  comparison.
- `RasterReadOptions` — band, overview level, sampling step, output type,
  memory budget, cancellation, and which of those participate in cache
  identity.
- `RandomAccessSource`, `ReadResult`, `MemorySource`, `LocalFileSource`,
  `RecordingSource` — the byte-source contract and its local implementations.

## Non-responsibilities

`usdRasterCore` does not parse any file format, does not decompress, does not
know what a TIFF is, does not author USD, does not decide tiling policy, and
does not perform HTTP. It opens no file except through `LocalFileSource`, which
exists so that a reader never has to.

It does not resample or reproject. `samplingStep` is decimation — taking every
n-th sample — not interpolation, and there is no resampling kernel here.

## Public API

| Header | Provides |
| --- | --- |
| `usdraster/RasterTypes.h` | `RasterSize`, `RasterDataType`, `IsExactlyRepresentable` |
| `usdraster/RasterWindow.h` | `RasterWindow`, `GetSampledExtent`, `GetSampledSize` |
| `usdraster/RasterGeoTransform.h` | `RasterGeoTransform`, `PixelAnchor`, `PixelCoord`, `TryGetWindowBounds` |
| `usdraster/RasterMetadata.h` | `RasterMetadata`, `RasterBandInfo` |
| `usdraster/RasterGrid.h` | `RasterGrid` |
| `usdraster/NoData.h` | `NoDataValue`, `NoDataPolicy` |
| `usdraster/RasterReadOptions.h` | `RasterReadOptions` |
| `usdraster/RandomAccessSource.h` | `RandomAccessSource`, `ReadResult`, `MemorySource`, `LocalFileSource`, `RecordingSource` |

### Minimal usage

```cpp
#include <usdraster/RasterGeoTransform.h>
#include <usdraster/RasterWindow.h>

// North-up, 2 m pixels, tiepoint at the upper-left corner of pixel (0, 0).
const auto transform = usdraster::RasterGeoTransform::FromPixelScaleAndTiepoint(
    2.0, 2.0, 0.0, 0.0, 300000.0, 4400000.0);

double x = 0.0, y = 0.0;
if (transform.TryPixelToSource(0, 0, usdraster::PixelAnchor::Area, x, y)) {
    // x == 300001.0, y == 4399999.0 -- the centre of pixel (0, 0).
}

// A window read of a 512x512 region, subdivided for bounded memory.
const usdraster::RasterWindow window{1000, 1000, 512, 512};
for (const auto& tile : window.Subdivide(256, 256)) {
    // Four tiles, covering the window exactly.
}
```

`TryPixelToSource` returns false for `PixelAnchor::Unknown`. That is the
point: a caller that has not resolved the anchoring cannot get a plausible
answer out of it.

## Dependencies

`usdgeo::core` only. Enforced by `tools/check_core_headers.py`, which runs as
the `core_dependency_check` CTest case. OpenUSD, TIFF and GeoTIFF types,
libtiff, and every transport library are forbidden here — section 2 of
[WORKSPACE.md](../../docs/architecture/WORKSPACE.md).

The installed package declares that edge too: `usdRasterCoreConfig.cmake`
issues `find_dependency(usdGeoCore)`, so a consumer calling only
`find_package(usdRasterCore)` resolves `usdgeo::core` transitively rather than
failing at generate time on an unresolved target.

## Data flow

```text
bytes  ->  RandomAccessSource  ->  format reader (usdGeoTiff, ...)
                                          |
                                          v
                          RasterMetadata / RasterGrid  ->  authoring, tiling
```

This module owns the contracts on both sides of a format reader and none of the
reader itself.

## Error and diagnostic behavior

There are no exceptions and no error returns from the value types; the
degenerate cases have defined results:

- A disjoint `Intersect` is the empty window, not a wrapped unsigned extent.
- `ClipTo` a window that starts past the extent is empty.
- `Subdivide` with a zero tile size yields nothing rather than dividing by zero.
- `RasterSize::GetPixelCount`, `RasterWindow::GetEndX` / `GetEndY`, and
  `RasterWindow::FromOverview` saturate rather than overflowing, so a malformed
  header claiming an enormous size produces a diagnosable number instead of a
  small wrapped one that then allocates successfully.
- `RasterGrid` reads and writes outside the sampled extent are ignored, not
  undefined.
- A `RasterGrid` whose sampled extent cannot be allocated is **empty** —
  `IsEmpty()` is true and `GetSize()` is zero — rather than throwing. Its
  `GetWindow()` still names the region that was requested, for the diagnostic.

Fallible operations return `bool` and are named `Try*`: `TryPixelToSource`,
`TrySourceToPixel`, `TryInverse`, `TryGetWindowBounds`. Each fails for a reason
its caller can name in a diagnostic — an unknown anchoring, a non-invertible
transform, a position outside the raster.

`RandomAccessSource::Read` returns a `ReadResult` distinguishing success, a
short read, end of source, and failure. **A short read is reported, never
retried inside a reader**: retry policy is transport policy, and transport
belongs to the resolver. See
[ADR-0002](../../docs/adr/0002-resolver-owns-transport.md).

Diagnostic codes classified as `RASxxx` belong to this module; see
[DIAGNOSTICS.md](../../docs/architecture/DIAGNOSTICS.md). `RasterWindow`
converts to the `usdgeo::PixelWindow` anchor a diagnostic carries.

## Threading and ownership

- The value types are values: independent copies, no shared state, no internal
  synchronization.
- **`RasterGrid` owns its sample buffer.** `GetSamples` returns a reference
  whose lifetime is the grid's; a caller needing the data longer copies it.
  Owning is the default and a non-owning view is added only if a measured copy
  cost justifies it.
- **`MemorySource` does not own its buffer.** The buffer must outlive the
  source. This is deliberately the opposite of `RasterGrid`: a fixture is a
  static array in a test, and copying every fixture to read it would make the
  instrumented byte counts measure the copy rather than the read.
- `LocalFileSource` owns its file handle and closes it on destruction.
- `RecordingSource` holds a reference to the source it wraps, which must
  outlive it.
- A `RandomAccessSource` implementation is **not required to be thread-safe**.
  A caller reading from several threads gives each thread its own source.

## Memory behavior

The reader surface is windowed: no public API here requires materializing a
whole image to satisfy a bounded request. Concretely:

| API | Bounded by |
| --- | --- |
| `RasterGrid` construction | the window's sampled size — `ceil(w/step) * ceil(h/step)` samples of `double` |
| `RasterWindow::Subdivide` | the tile count, one small struct each |
| `RandomAccessSource::Read` | the caller's buffer, which must hold `size` bytes; the source allocates nothing |
| `RasterMetadata` | the band and overview counts; no pixel data |

`RasterReadOptions::memoryBudgetBytes`, when non-zero, is what a reader
enforces. A budget that could be exceeded "just this once" is not a budget: a
request that cannot be satisfied within it fails with a diagnostic naming the
size it needed. Nothing in this module allocates in proportion to the source
size, only to the requested window.

## Coordinate spaces

This is the section that prevents the defect a "width" parameter that is
sometimes pixels and sometimes metres produces.

| Value | Space |
| --- | --- |
| `RasterWindow`, `RasterSize` | source **pixels**, always at **full resolution** |
| `RasterGrid::GetSize` | **sampled-grid** coordinates, after `samplingStep` |
| `RasterGrid::GetSample(column, row)` | sampled-grid, **not** source pixels |
| `RasterGrid::GetSourcePixel` | converts sampled-grid to source pixels |
| `PixelCoord` | continuous **pixel space**, after anchoring is applied |
| `RasterGeoTransform` output | **source coordinates**, in the source CRS |
| `TryGetWindowBounds` result | source coordinates; Z is degenerate at zero |

Three rules follow, and each is covered by a test:

1. A window is always full-resolution, whichever overview serves it. A caller
   never rescales its own request.
2. A sampling step never re-anchors the grid. Sample `(0, 0)` is always the
   window's first pixel, which is what keeps a coarse mesh registered with a
   fine one.
3. Pixel anchoring is explicit at every pixel-to-source boundary and is never
   guessed. `PixelAnchor::Unknown` is a value, and the conversion refuses it.

See [COORDINATE_MODEL.md](../../docs/architecture/COORDINATE_MODEL.md) and
[RASTER_READER.md](../../docs/architecture/RASTER_READER.md).

## Build and test

```text
cmake -S . -B build-core -DUSDRASTER_CORE_ONLY=ON
cmake --build build-core
ctest --test-dir build-core -R usdRasterCore
```

The expected values in the tests are hand-computed from the formulas in the
coordinate contract, not captured from a previous run: a golden value produced
by the code under test proves only that the code is stable.

## Known limitations

- Sub-byte sample formats (1, 2, and 4 bits) are not in `RasterDataType`. An
  unused enumerator is a contract nothing tests; they enter when a format needs
  them.
- `RasterGrid` holds `double` samples regardless of the source type. For a
  `uint8` source that is eight times the source's own footprint. The window
  bounds it, and one sample type downstream is worth more than the saving; if a
  measurement changes that, the type becomes a parameter.
- `LocalFileSource` performs no read-ahead, caching, or coalescing. That is
  deliberate — it is the baseline a remote source is measured against — but it
  means a naive caller issuing many small reads pays for each.
- `RecordingSource` records unboundedly. It is for tests and for bounded
  conversion runs, not for a long-lived session.
- There is no resampling. `samplingStep` decimates.

## Planned work

| Item | Status | Milestone |
| --- | --- | --- |
| Value model, window arithmetic, geotransform, NoData, sources | implemented | 1 |
| `ReadWindow` / `ReadTile` / `ReadScanlines` reader interface | implemented | 3 |
| Read planning and range coalescing | implemented | 3 |
| I/O amplification counters on a read result | implemented | 3 |
| Memory budget enforcement | planned | 3 |
| `ArAssetSource`, in the plugin bundle rather than here | planned | 8 |

The task-level record is
[implementation-status.md](../../docs/roadmap/implementation-status.md).
