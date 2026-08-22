# ADR-0002: The Resolver Owns Transport

## Status

Accepted for Milestone 0. Binding on every module.

## Decision

`usd-raster-plugins` implements no transport. It reads bytes through a
project-owned `RandomAccessSource`, and the plugin bundle adapts an OpenUSD
`ArAsset` onto that interface.

```text
raster-geotiff -> ArAsset::Read(offset, size)
                       -> ArResolver
                       -> usd-http-resolver
                       -> HTTP Range
```

Out of scope in this repository, permanently:

```text
HTTP, HTTPS, TLS
Range request construction
authentication, credentials, signed URLs
retry, backoff, timeout policy
connection pooling
raw byte caching
URL or scheme parsing
```

Correspondingly, a resolver never learns about TIFF, IFDs, geotransforms, or
USD authoring.

## Rationale

The boundary is not an aesthetic preference; it is what keeps both sides
finite.

- Transport implemented in a plugin is transport implemented once per plugin.
  A second raster format, or the point-cloud repository, would each need their
  own.
- Retry and caching policy interact with the host application and with
  credentials. Encoding them in a format reader makes them unconfigurable by
  the people who need to configure them.
- OpenUSD already defines the extension point. `ArResolver` exists precisely so
  that asset access is pluggable, and duplicating it inside a FileFormat plugin
  fights the composition model rather than using it.
- A reader that reads byte ranges honestly gets remote support for free. The
  work that makes GeoTIFF stream well — windowed reads, tile-aware planning,
  overview selection — is the same work that makes it fast locally.

## Consequences

- Remote GeoTIFF support is a property of the composed system, not a feature of
  this repository. It works when the active resolver provides efficient
  random-access `ArAsset` reads.
- No resolver implementation is a build-time dependency. The CI gate must pass
  with no external resolver present, using an in-repository test resolver
  fixture.
- Short reads and unavailable assets are reported as typed diagnostics rather
  than retried. A caller that wants retries configures the resolver.
- Byte caching and generated-USD caching stay separate, with separate keys and
  separate lifetimes. See [CACHE.md](../architecture/CACHE.md).
- COG support requires no HTTP code here. It is overview discovery and
  tile-aware reads inside the GeoTIFF reader; see
  [ADR-0004](0004-geotiff-first-format.md).

## Revisiting

Not revisited. If a transport need cannot be met by a resolver, the fix belongs
in the resolver or in a new resolver, never in a FileFormat plugin.
