# Generated Cache Contract

Two caches exist in the composed system, and they are not the same cache:

```text
usd-http-resolver     raw byte cache      "these bytes came from that URL"
usd-raster-plugins    generated cache     "this USD was authored from that source
                                           under those arguments"
```

This document covers the second. The first is out of scope and is never
reimplemented here.

Status: deferred until the converter exists. The contract is recorded now
because the cache key must be derivable from values the earlier milestones
already produce, and retrofitting identity is expensive.

## 1. What is cached

Authored output that is expensive to produce and deterministic to reproduce:

- tiled payload `.usdc` files and their root layer;
- the conversion manifest;
- large single-mesh outputs produced by the converter.

Not cached: decoded pixel buffers, source bytes, or anything a resolver already
holds.

## 2. Cache key

The key is the normalized concatenation of everything that can change the
output:

```text
source identity
+ normalized file-format arguments
+ representation
+ sampling profile and applied step
+ tiling configuration
+ NoData policy
+ local origin
+ coordinate contract version
+ authoring library version
+ plugin version
= deterministic cache key
```

Two facts about this list matter more than the rest:

**Normalized arguments, not raw ones.** `lod=BALANCED` and `lod=balanced` are
the same entry. `heightScale=1` and `heightScale=1.0` are the same entry. That
requires normalization to happen before lookup, which is why
[FILE_FORMAT_ARGUMENTS.md](FILE_FORMAT_ARGUMENTS.md) puts it first in the
plugin flow.

**Versions are in the key.** A change to the coordinate contract, the authoring
code, or the profile table changes the output, so it must change the key. A
cache that survives a behavior change serves wrong geometry, which is worse
than no cache.

## 3. Source identity

| Source | Identity inputs |
| --- | --- |
| Local file | resolved path, size, modification time |
| Resolver-provided | resolved identifier, size, resolver validation token or digest |

Reuse is conservative: when identity inputs are not sufficiently stable, reuse
is **disabled** and the conversion proceeds from the source with a warning.
This is a deliberate asymmetry — a missed cache hit costs time, a false cache
hit produces silently wrong terrain.

No credential, authorization header, signed URL, or token is ever persisted
into a cache descriptor. Tokens are compared as opaque values, never parsed.
See [RESOLVER_SOURCE.md](RESOLVER_SOURCE.md).

## 4. Layout and publication

```text
<cache root>/<key>/
    root.usda
    tiles/L0/0_0.usdc
    tiles/L0/0_1.usdc
    manifest.json
```

An entry is published atomically: the conversion writes into a temporary
directory and renames it into place only after the manifest is complete. A
reader therefore never observes a half-written entry, and an interrupted
conversion leaves a temporary directory that cleanup can remove without
ambiguity.

The cache root is configured by environment variable, following the convention
of `usd-pointcloud-plugins`.

## 5. Lookup states

Lookup returns a machine-readable state, not a boolean:

```text
Hit                   a complete, valid entry was found and reused
Miss                  no entry for this key
StaleIdentity         an entry exists but source identity changed
IdentityUnavailable   identity is too weak to trust; reuse disabled
Corrupt               an entry exists but fails validation; quarantined
Disabled              caching is turned off for this run
```

Distinguishing these is what makes a cache debuggable. "It did not reuse" is
not an actionable report; "identity unavailable because the resolver supplied
no validation token" is.

Process-local statistics — hits, misses, bytes reused, time saved — are
reported by the converter.

## 6. Corruption and recovery

An entry that fails validation is quarantined rather than deleted silently, and
the conversion proceeds as a miss. Validation checks the manifest against the
files it names: every declared payload exists, has the declared size, and
belongs to the declared key.

## 7. What the FileFormat plugin does

The direct read path **reuses** committed entries; it does not generate them.
Generation is the responsibility of the converter, because generation is the
long-running operation that
[ADR-0008](../adr/0008-preview-vs-converter.md) keeps out of an interactive
read.

A plugin read that misses the cache and exceeds its interactive ceiling fails
with a diagnostic naming the converter, rather than starting a conversion the
host cannot cancel meaningfully.
