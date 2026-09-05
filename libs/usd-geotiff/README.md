# usdGeoTiff

## Purpose

`usdGeoTiff` reads TIFF and GeoTIFF structure through `RandomAccessSource` and
returns `RasterMetadata` or a bounded `RasterGrid` window. The metadata path
reads headers, IFDs, and metadata value arrays without reading pixel segments.

## Non-responsibilities

This library does not author USD, perform HTTP, decide tiling policy, or create
materials. Mesh authoring is a subsequent milestone.

## Status

The classic and BigTIFF metadata path is implemented and tested for the
synthetic fixtures. Chunky and separate-planar strip and tile windows are
implemented for UInt16 and Float32, including sampled reads,
endian conversion, GDAL band descriptions, units, scale, and offset. Deflate
windows are implemented through the optional libtiff backend. LZW and PackBits
are also available through that backend. Horizontal differencing predictors
are supported for integer and floating-point samples. Uncompressed reads plan
intersecting segments and coalesce adjacent ranges up to 64 KiB; callers may
collect requested bytes, fetched bytes, request count, and amplification via
`RasterReadStatistics`. Explicit overview levels and automatic selection based
on `samplingStep` read the selected IFD while preserving full-resolution window
coordinates.