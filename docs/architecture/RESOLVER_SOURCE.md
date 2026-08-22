# Resolver-backed source contract

This document fixes the boundary between `usd-raster-plugins` and any
`ArResolver` implementation, including `usd-http-resolver`.

The rule it exists to protect:

**A FileFormat Plugin does not know about transport. A resolver does not know
about formats.**

See [ADR-0002](../adr/0002-resolver-owns-transport.md).

## 1. Responsibility boundary

| Concern | Owner |
| --- | --- |
| HTTP, HTTPS, TLS | resolver |
| Range requests | resolver |
| Authentication, credentials, signed URLs | resolver |
| Retry, backoff, timeout | resolver |
| Raw byte cache | resolver |
| Connection pooling, transport metrics | resolver |
| TIFF structure, IFDs, tiles, strips | this repository |
| Which byte ranges are needed | this repository |
| Read coalescing and window planning | this repository |
| Generated USD cache | this repository |
| CRS, geotransform, NoData, sampling | this repository |

The interesting line is between "which bytes" and "how the bytes arrive". The
reader decides which bytes a window needs and asks for them; everything about
how they arrive is on the other side of `ArAsset`.

## 2. The interface

```text
ArResolver::OpenAsset(resolvedPath)
      -> ArAsset
      -> ArAssetSource : public RandomAccessSource   (plugin bundle)
      -> GeoTiffReader                               (usdGeoTiff)
```

`ArAssetSource` is the only OpenUSD-aware source implementation, and it lives
in the plugin bundle. `usdGeoTiff` never sees an `ArAsset`, a URL, or a scheme.

Requirements on the adapter:

- deterministic offset reads;
- source size discovery;
- explicit short-read and EOF results;
- no retries, no caching, no scheme inspection.

Local reads use the same interface, so remote reads are not a parallel path.
This is the property that keeps remote support from decaying: a bug in the
remote path is a bug in the only path.

## 3. What makes GeoTIFF work remotely

Nothing special, if the reader is honest about byte ranges:

```text
open a remote DEM, representation=metadata
    -> read the TIFF header               (~8 bytes)
    -> read the first IFD                 (a few hundred bytes)
    -> read the GeoTIFF key directory     (a few hundred bytes)
    = a few kilobytes, over 2-3 Range requests
```

```text
open the same DEM, representation=mesh, lod=preview
    -> header + IFDs
    -> select an overview level if present
    -> read only the tiles intersecting the requested window
```

A tiled GeoTIFF or COG therefore streams naturally. A striped GeoTIFF does not
stream as well, because a strip is a full row run; that is a property of the
source, and the reader reports the resulting I/O amplification rather than
hiding it. See [RASTER_READER.md](RASTER_READER.md).

## 4. Source identity

Generated-USD cache reuse needs stable identity for the source. The plugin
derives it from what the resolver already provides, and never from transport
internals:

| Input | Stability |
| --- | --- |
| Resolved identifier | required |
| Source size | required |
| Resolver-provided validation token, digest, or version | preferred |
| Modification time | weak; local only |

Reuse is conservative. When identity inputs are not sufficiently stable, cache
reuse is **disabled** and the read proceeds from the source, with a warning.
Serving stale generated geometry is worse than reading again.

Tokens are opaque. The plugin compares them; it never parses them, and it never
persists a credential, an authorization header, or a signed URL into a
manifest, a cache descriptor, or a diagnostic.

## 5. Cache ownership

```text
usd-http-resolver     raw HTTP block cache
usd-raster-plugins    generated / decoded representation cache
```

These are different caches with different keys and different lifetimes. The
resolver caches bytes it fetched; this repository caches USD it authored. See
[CACHE.md](CACHE.md).

## 6. Build-time independence

No resolver implementation is a build-time dependency of this workspace. There
is no CMake dependency, submodule, vendored transport library,
resolver-specific include, or link dependency on one. External resolvers
compose at runtime through the standard OpenUSD plugin mechanism.

The required CI gate must stay passable with no external resolver repository
present.

## 7. Testing tiers

**Tier 1 — repository-local, required.** An in-repository test `ArResolver`
serves a fixture from memory for a synthetic scheme. It has no network
transport and no product bundle manifest, is built only with the integration
tests, and is excluded from product discovery and release matrices. It proves:

- selective reads: header-only inspection touches kilobytes, not the file;
- equivalence: local file, memory source, and resolver-provided asset produce
  identical authored stages;
- failure mapping: a short read or an unavailable asset produces the documented
  diagnostic.

An instrumented source records every requested range, so a regression that
turns a windowed read back into a whole-file read fails a test rather than a
benchmark.

**Tier 2 — cross-repository, composed separately.** Integration against a real
`usd-http-resolver` build, run outside the required gate of this repository.
It validates end-to-end remote GeoTIFF reads and the byte-range behavior
against a real server.

## 8. Interoperability target

```usda
def "Terrain" (
    references = @https://example.com/dem.tif:SDF_FORMAT_ARGS:representation=mesh&lod=preview@
)
{
}
```

Nothing in this repository parses that URL. The plugin receives whatever
`ArResolver` resolved it to, opens an `ArAsset`, and reads byte ranges. If that
works, remote raster works — and if it does not, the fix belongs in the
resolver.
