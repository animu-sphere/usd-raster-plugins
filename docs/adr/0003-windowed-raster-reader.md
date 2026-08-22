# ADR-0003: Windowed, Random-Access Raster Reader

## Status

Accepted for Milestone 0. Binding on `usdRasterCore` and every format reader.

## Decision

The central reader API is windowed. No public API requires materializing a
whole image to satisfy a bounded request.

```cpp
// the contract
ReadWindow(window, options);
ReadTile(tileId, options);
ReadScanlines(firstRow, rowCount, options);

// forbidden as a central API
std::vector<float> ReadAllPixels();
```

A window is expressed in full-resolution source pixel coordinates regardless of
which overview level serves it, so a caller never rescales its request.

A convenience that reads an entire small raster may exist, but it is a caller
of `ReadWindow` with an explicit size check, not the primitive.

## Rationale

Whole-image APIs are irreversible design decisions disguised as convenience.
Once callers depend on one, the capabilities it forecloses cannot be restored:

- **Memory.** A 40000 x 40000 float32 DEM is 6.4 GB. Bounded-memory conversion
  becomes impossible when the primitive is the whole image.
- **Remote sources.** A whole-image read must transfer the entire file before
  producing anything, which makes Range requests, COG overviews, and
  `ADR-0002` pointless.
- **Tiling.** The converter processes one spatial tile at a time. That
  requires a source primitive smaller than the source.
- **Preview.** Inspecting a large DEM should cost kilobytes. With a whole-image
  primitive, preview costs the same as production.

The reverse direction is cheap: a whole-image read is trivially expressible as
one window. Choosing the smaller primitive costs nothing and preserves
everything.

## Consequences

- The reader must plan reads: map a window onto intersecting native tiles or
  strips, coalesce adjacent byte ranges, and report requested versus fetched
  bytes so I/O amplification is visible.
- Strip-organized sources cannot serve a narrow window cheaply, because a strip
  is a run of full rows. This is reported, not hidden.
- Decode buffers are bounded by tile or strip size, not by image size.
- Tests can use small in-memory fixtures for behavior that would otherwise
  require large files.
- Callers carry slightly more responsibility: they choose windows. The tiling
  module exists so that most callers do not choose them by hand.

## Related

- [RASTER_READER.md](../architecture/RASTER_READER.md) — the full contract.
- [ADR-0002](0002-resolver-owns-transport.md) — why byte-range honesty matters.
- [TILING.md](../architecture/TILING.md) — who plans windows in practice.
