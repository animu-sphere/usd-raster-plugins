# ADR-0007: GDAL Is Not a Core Runtime Dependency

## Status

Accepted for Milestone 0.

## Decision

GDAL is not linked into any production target of this repository. It may be
used as an optional **test oracle** and as an optional fixture-generation tool.

```text
production   usdGeoTiff -> libtiff -> RandomAccessSource
test only    oracle comparison against GDAL, in an optional test target
```

An optional GDAL-backed reader is not ruled out forever, but it would be an
additional, clearly separated module, never the core path.

## Rationale

GDAL is excellent at what it does, and what it does is broader than what this
repository should own:

- **Responsibility overlap.** GDAL owns format handling, CRS handling, I/O, and
  a virtual filesystem. Three of those four are owned elsewhere in this
  architecture: I/O and the virtual filesystem belong to `ArResolver`, and CRS
  interpretation is deliberately minimal here. Adopting GDAL means either
  duplicating those responsibilities or ceding the boundary that
  [ADR-0002](0002-resolver-owns-transport.md) protects.
- **Transport leakage.** GDAL `/vsicurl/` would fetch bytes itself, bypassing
  the resolver entirely. The composed system would then have two transport
  stacks with two caches and two credential paths, and the resolver would stop
  being authoritative.
- **Dependency size.** GDAL pulls a large transitive graph. For a plugin
  intended to be redistributed into DCC and renderer environments, that is a
  real deployment cost, and it constrains the pinned runtime.
- **Determinism.** The output contract here is byte-stable authored USD. That
  is easier to guarantee over a narrow decoding backend than over a large stack
  with its own configuration, environment variables, and driver auto-detection.

At the same time, GDAL is the best available check on correctness. Comparing
decoded windows, geotransforms, and CRS interpretation against GDAL catches the
class of bug that golden fixtures cannot: a fixture encodes what this
implementation believes, and an oracle encodes what the ecosystem believes.

## Consequences

- Production and test dependencies are separated. No production CMake target,
  manifest, or package declares GDAL.
- Oracle tests are optional and skipped when GDAL is unavailable, so the
  required CI gate never depends on it.
- This repository implements TIFF and GeoTIFF decoding through libtiff and
  accepts the cost of covering compressions and sample formats explicitly. The
  capability matrix is therefore narrower than GDAL, on purpose, and honestly
  stated.
- Users who need format breadth beyond the capability matrix convert with GDAL
  first. That is a legitimate workflow and is documented rather than treated as
  a gap.

## Revisiting

Reconsider only if a needed capability — a compression, a CRS interpretation —
proves impractical to implement and there is no narrower library that provides
it. Even then, the result would be an optional module, not a core dependency.
