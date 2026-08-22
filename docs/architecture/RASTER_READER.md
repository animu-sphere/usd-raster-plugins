# Raster Reader Architecture

`usdRasterCore` owns the format-independent reader contract that `usdGeoTiff`
and every later format reader implements. The central rule is that the reader
is **windowed**: no public API requires materializing a whole image to satisfy
a bounded request. See
[ADR-0003](../adr/0003-windowed-raster-reader.md).

## 1. Why windowed

A whole-image API removes capabilities that cannot be added back later:

```cpp
std::vector<float> ReadAllPixels();     // forbidden as a central API
```

- A 40000 x 40000 float32 DEM is 6.4 GB. Bounded memory becomes impossible.
- A remote source must download everything before producing anything, so
  Range requests and COG overviews become pointless.
- Tiled authoring cannot stream, so the converter has to hold the whole source.
- Preview cannot be cheaper than production.

The windowed surface instead:

```cpp
ReadWindow(window, options)      // arbitrary rectangle
ReadTile(tileId, options)        // a native tile, no resampling
ReadScanlines(first, count, ...) // a strip-oriented run of rows
```

Every one of these is expressible as a bounded byte range over the source, so
the same reader works for a local file, an in-memory fixture, and a
resolver-provided remote asset.

## 2. Value model

```cpp
struct RasterSize {
    uint64_t width;
    uint64_t height;
};

struct RasterWindow {
    uint64_t x;        // leftmost column, in source pixels
    uint64_t y;        // topmost row, in source pixels
    uint64_t width;
    uint64_t height;
};
```

A window is expressed in **source pixel coordinates at the full-resolution
level**, regardless of which overview level ultimately serves it. A caller
therefore never has to rescale its request when overview selection changes.

```cpp
enum class RasterDataType {
    UInt8, Int8, UInt16, Int16, UInt32, Int32,
    Float32, Float64
};

struct RasterBandInfo {
    uint32_t         index;          // 1-based, matching GeoTIFF convention
    RasterDataType   dataType;
    std::optional<double> noDataValue;
    std::optional<double> scale;
    std::optional<double> offset;
    std::string      description;
    std::string      unit;
};
```

`RasterGrid` is the decoded result of a window read: a contiguous buffer plus
the window it came from, the band it came from, the data type, and the NoData
value in force.

```cpp
struct RasterMetadata {
    RasterSize                    size;
    std::vector<RasterBandInfo>   bands;
    RasterGeoTransform            geoTransform;
    PixelAnchor                   pixelAnchor;
    CrsDescription                crs;
    std::optional<RasterSize>     nativeTileSize;   // empty when strip-organized
    std::vector<RasterSize>       overviewSizes;
    GeoBounds                     bounds;
};
```

Metadata is readable without decoding a single pixel. That is what makes
`representation=metadata` cheap and what makes inspecting a multi-gigabyte
remote DEM practical.

## 3. Read options

```cpp
struct RasterReadOptions {
    uint32_t                 band = 1;
    std::optional<uint32_t>  overviewLevel;      // empty: chosen by the reader
    uint64_t                 samplingStep = 1;
    RasterDataType           outputType = RasterDataType::Float32;
    uint64_t                 memoryBudgetBytes = 0;   // 0: caller-managed
    std::function<bool()>    isCancelled;
};
```

- `band` selects one band. Multi-band reads are a separate call, not a hidden
  fan-out, so memory stays predictable.
- `overviewLevel`, when empty, lets the reader pick the coarsest level that
  still satisfies the requested `samplingStep` without losing detail the caller
  asked for. An explicit level disables that choice and is used by tests and
  by the converter.
- `samplingStep` is decimation in source pixels. A step never re-anchors the
  grid; sample `(0, 0)` is always source pixel `(0, 0)`. See
  [COORDINATE_MODEL.md](COORDINATE_MODEL.md).
- `memoryBudgetBytes`, when non-zero, bounds decode buffers. A request that
  cannot be satisfied within the budget fails with a diagnostic naming the
  required size; it does not silently allocate more.
- `isCancelled`, when provided, is checked at window and tile boundaries. A
  true result stops the read with a diagnostic and releases partial buffers.

Options that change decoded values participate in cache identity. Options that
only change scheduling — cancellation, budget — do not.

## 4. Source access

The reader consumes bytes through a project-owned interface and never opens a
file itself:

```cpp
class RandomAccessSource {
public:
    virtual ~RandomAccessSource() = default;
    virtual uint64_t   GetSize() const = 0;
    virtual ReadResult Read(uint64_t offset, size_t size, void* dst) = 0;
};
```

`ReadResult` distinguishes success, short read, end of source, and failure.
Short reads are reported, never retried inside the reader: retry policy is
transport policy and belongs to the resolver.

Implementations:

```text
LocalFileSource       a local file
MemorySource          an in-memory fixture, used by tests
ArAssetSource         an OpenUSD ArAsset, provided by the active resolver
```

`ArAssetSource` lives in the plugin bundle, because it is the only one that
needs OpenUSD. `usdGeoTiff` never sees an `ArAsset`, a URL, or a scheme. See
[RESOLVER_SOURCE.md](RESOLVER_SOURCE.md).

## 5. Read planning and I/O amplification

A windowed request maps onto source structure differently depending on layout:

```text
tiled TIFF   -> the intersecting native tiles only
striped TIFF -> the intersecting strips, whole rows each
```

A striped source therefore reads far more bytes than a narrow window needs.
That is a property of the source, not a defect, and the reader reports it:
every read exposes requested bytes, fetched bytes, and the resulting
amplification ratio. Those counters are what make a remote-source regression
visible before it becomes a performance complaint.

The reader plans reads before issuing them, so that adjacent byte ranges are
coalesced into one `Read` call where the gap between them is smaller than a
configured threshold. Coalescing is a reader decision; caching those bytes is
not, and belongs to the resolver.

## 6. Decoding and compression

Decompression happens per strip or per tile, inside the decoding backend, with
buffers bounded by the strip or tile size rather than by the image size.
Unsupported compression is a typed diagnostic naming the compression code, not
a generic failure.

The decoded output type is `outputType`, converted from the source sample
format. Conversions that cannot be exact — for example `Float64` to `Float32`
on a source with large elevation offsets — are permitted but recorded. Integer
sources with band scale and offset are converted through `double` before
narrowing.

## 7. Reachability

A capability that exists in a library but that no argument reaches is stated as
`implemented, not connected`, never as `supported`. The status vocabulary is
fixed in
[MODULE_README_CONTRACT.md](../contributing/MODULE_README_CONTRACT.md).

The reader learns nothing about LOD profiles, cameras, viewport state, tiling
policy, or USD. It decodes a requested window and reports what it did. Mapping
a profile onto a sampling step happens in the plugin adapter; mapping a grid
onto USD happens in `usdRasterAuthoring`.

## 8. Threading and ownership

A `RasterReader` instance is not thread-safe; concurrent reads use separate
instances over separate sources. Returned buffers are owned by the returned
`RasterGrid` value and remain valid independently of the reader, so a caller
may hand a grid to an authoring stage while the reader continues.

A `RandomAccessSource` implementation must document its own thread-safety.
`ArAssetSource` follows whatever the active resolver guarantees, which is why
the reader does not assume concurrent access.
