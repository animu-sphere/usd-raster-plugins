# Diagnostics Contract

Section 7 of the [design policy](../design/DESIGN_POLICY.md) requires typed
diagnostics instead of string-only errors. This document fixes the value types,
the code ownership, and the anchors a raster diagnostic must carry.

The value types in section 1 are implemented in `usdGeoCore`. The code tables
in sections 3 and 5 are the target allocation: a code is published when its
condition becomes reachable and has a fixture, so most are still planned.

## 1. Value types

`usdGeoCore` owns the diagnostic value types so every reader, the tiling
module, the authoring library, and every plugin share them.

```cpp
enum class Severity { Warning, Error };

enum class DiagnosticCode {
    // source and container
    InvalidSignature,
    UnsupportedVersion,
    TruncatedHeader,
    TruncatedData,
    InvalidOffset,
    ShortRead,
    SourceUnavailable,

    // raster structure
    UnsupportedCompression,
    UnsupportedSampleFormat,
    UnsupportedPlanarConfiguration,
    InvalidRasterSize,
    InvalidBandIndex,
    InconsistentTileLayout,

    // georeferencing
    MissingGeoreference,
    InvalidGeoTransform,
    ConflictingGeoTransform,
    UnknownPixelAnchor,
    InvalidCrs,
    ConflictingCrs,
    UnsupportedCrs,

    // values
    NonFiniteValue,
    InvalidNoDataValue,
    LossyConversion,

    // request
    WindowOutOfBounds,
    UnsupportedOverviewLevel,
    MemoryBudgetExceeded,
    Cancelled,

    // arguments
    UnknownFormatArgument,
    UnsupportedFormatArgument,
    InvalidFormatArgument,
    ConflictingFormatArguments,

    // authoring
    AuthoringFailed,
    VertexBudgetExceeded
};

// A rectangle in source pixel coordinates. Deliberately not RasterWindow:
// that type belongs to usdRasterCore, and usdGeoCore may not depend on it.
struct PixelWindow {
    std::uint64_t x, y, width, height;
};

struct Diagnostic {
    DiagnosticCode code;
    Severity       severity;
    std::string    message;

    // raster anchors; present when meaningful
    std::optional<std::uint64_t> byteOffset;
    std::optional<std::uint64_t> pixelX;
    std::optional<std::uint64_t> pixelY;
    std::optional<PixelWindow>   window;
    std::optional<std::uint32_t> band;
};
```

The anchors are the raster-specific part. A point-cloud diagnostic anchors to a
point index; a raster diagnostic anchors to a pixel, a window, or a band, and a
message without an anchor is much harder to act on when the source is 40000
pixels wide.

The window anchor is `PixelWindow` rather than `RasterWindow` because of the
dependency direction. `usdGeoCore` owns the diagnostic types and, by section 2
of [WORKSPACE.md](WORKSPACE.md), depends on nothing — while `RasterWindow`
belongs to `usdRasterCore`, which depends on `usdGeoCore`. Using `RasterWindow`
here would invert that edge. So the anchor is its own plain rectangle carrying
no arithmetic, and `RasterWindow::ToAnchor` converts. The duplication is four
integers; the alternative is a dependency cycle.

## 2. Rules

1. `DiagnosticCode` values are stable once published. A name is never reused
   for a different meaning.
2. Every failure path produces a code. There is no generic `Unknown`.
3. A warning must leave a stage that can be opened. If the condition prevents a
   valid stage, it is an error, not a warning.
4. Lossy behavior that the caller asked for is a warning plus authored
   metadata, not a silent success. `LossyConversion` covers vertical
   exaggeration, narrowing conversions, and NoData substitution.
5. A diagnostic message never contains a credential, a token, an authorization
   header, or a signed URL. A remote identifier is included only in the form the
   resolver already exposed.
6. `Cancelled` is a distinct code, because a cancelled read is not a failed
   read and hosts need to tell them apart.

## 3. Code ownership

| Prefix | Owner | Scope |
| --- | --- | --- |
| `GEOxxx` | `usdGeoCore` | CRS, transform, units, bounds, identity, cache |
| `RASxxx` | `usdRasterCore` | window, band, sample type, NoData, read options |
| `TIFxxx` | `usdGeoTiff` | TIFF/GeoTIFF container and decoding |
| `GTIFxxx` | `raster-geotiff` | user-facing plugin codes for `.tif` / `.tiff` |

Library codes classify; plugin codes are what a user sees. The plugin maps a
library code to its own stable code rather than inventing a new classification.

## 4. Presentation

A plugin diagnostic reaches OpenUSD through `TF_RUNTIME_ERROR` or
`TF_WARN` with the code, the source, and the available anchor:

```text
[GTIF004] Unable to read terrain.tif: unsupported compression (code 34887)
[GTIF007] terrain.tif: conflicting georeferencing, ModelTransformationTag and
          ModelTiepointTag disagree; using ModelTransformationTag
[GTIF012] terrain.tif: mesh representation needs 16777216 vertices, above the
          interactive limit of 4194304; use usd-raster-convert or lod=preview
```

The third example is the shape a good diagnostic takes: what was requested,
what the limit is, and what the caller can do instead.

## 5. Planned code table

The table below is the initial allocation. Codes are added as their conditions
become reachable; an allocated-but-unreachable code is not published.

| Code | Severity | Condition |
| --- | --- | --- |
| `GTIF001` | Error | The source is not a TIFF, or the header is truncated |
| `GTIF002` | Error | Unsupported TIFF or BigTIFF variant |
| `GTIF003` | Error | Malformed IFD or invalid tag offset |
| `GTIF004` | Error | Unsupported compression |
| `GTIF005` | Error | Unsupported sample format or bit depth |
| `GTIF006` | Error | No usable georeferencing and no explicit argument |
| `GTIF007` | Warning | Conflicting georeferencing tags |
| `GTIF008` | Error | Invalid or non-invertible geotransform |
| `GTIF009` | Warning | Pixel anchoring absent, supplied by argument |
| `GTIF010` | Error | Invalid band index for the source |
| `GTIF011` | Error | Requested window is outside the raster |
| `GTIF012` | Error | Authored vertex count exceeds the interactive limit |
| `GTIF013` | Error | Invalid, unknown, or conflicting file-format argument |
| `GTIF014` | Error | Source read failed or returned short |
| `GTIF015` | Warning | Lossy conversion applied, recorded in metadata |
| `GTIF016` | Error | Memory budget cannot satisfy the request |
| `GTIF017` | Warning | Read cancelled by the host |
| `GTIF018` | Error | Invalid GDAL NoData value |
| `GTIF019` | Error | Metadata authoring failed |

## 6. Testing

Each published code has a fixture that produces it. A code without a test is
not published, because an untested error path is usually a crash path.

The fixtures live beside the golden coordinate fixtures and are generated by
the same checked-in script, so a malformed-input fixture is reviewable as code.
