# usdGeoTiff

## Purpose

`usdGeoTiff` reads TIFF and GeoTIFF structure through `RandomAccessSource` and
returns `RasterMetadata`. The metadata path reads headers, IFDs, and metadata
value arrays without reading pixel segments.

## Non-responsibilities

This library does not author USD, perform HTTP, decide tiling policy, or create
materials. Pixel decoding and window reads are subsequent milestones.

## Status

The classic and BigTIFF metadata path is implemented and tested for the
synthetic fixtures. GeoTIFF keys, geotransforms, CRS basics, NoData, and native
tile layout are included; broader validation and pixel decoding are planned.