# usdGeoTiff

## Purpose

`usdGeoTiff` reads TIFF and GeoTIFF structure through `RandomAccessSource` and
returns `RasterMetadata` or a bounded `RasterGrid` window. The metadata path
reads headers, IFDs, and metadata value arrays without reading pixel segments.

## Non-responsibilities

This library does not author USD, perform HTTP, decide tiling policy, or create
materials. Overview selection and mesh authoring are subsequent milestones.

## Status

The classic and BigTIFF metadata path is implemented and tested for the
synthetic fixtures. Initial uncompressed chunky strip and tile windows are
implemented for UInt16 and Float32, including sampled reads, endian conversion,
GDAL band descriptions, units, scale, and offset. Deflate windows are
implemented through the optional libtiff backend. LZW, PackBits, separate
planar data, and overview selection remain planned.