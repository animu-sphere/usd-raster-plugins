# Module README contract

Every directory under `libs/`, `plugins/`, and `tools/` contains a `README.md`.
That README is part of the module contract, not optional supplementary
documentation: it is where the module states what it owns, what it refuses to
own, and what a caller may rely on.

This is invariant 15 of the
[workspace contract](../architecture/WORKSPACE.md).

## Ownership

A code change that modifies a module contract updates the `README.md` of that
module in the same pull request. Examples:

- adding or removing a public API;
- changing a file-format argument;
- changing supported compressions, sample formats, or georeferencing paths;
- changing coordinate handling or pixel anchoring behavior;
- changing NoData policy;
- changing ownership or lifetime rules for returned buffers;
- adding or removing a runtime dependency;
- changing license obligations.

A reviewer treats a contract change without a README change as incomplete.

## Required sections: library README

```text
# Module name

## Purpose
## Responsibilities
## Non-responsibilities
## Public API
## Dependencies
## Data flow
## Error and diagnostic behavior
## Threading and ownership
## Build and test
## Known limitations
## Planned work
```

A library README must additionally:

- state whether OpenUSD is required;
- state whether the module can be tested without an OpenUSD runtime;
- identify ownership and lifetime rules for returned buffers;
- document coordinate-space assumptions, including whether a value is in
  pixel, source, or stage-local space;
- state the memory behavior of any read API — what bounds it, and what does
  not;
- link to the relevant architecture contracts;
- include a minimal usage example where practical.

The coordinate-space requirement is not boilerplate. A function taking a
"width" that is sometimes pixels and sometimes metres is the defect this
section prevents.

## Required sections: plugin README

```text
# Plugin name

## Purpose
## Supported file extensions
## Supported formats, compressions, and sample formats
## FileFormat arguments
## Authored OpenUSD result
## Cost and limits
## Plugin discovery and installation
## Build and test
## Runtime dependencies
## Licensing
## Known limitations
## Compatibility
```

A plugin README must clearly distinguish:

- what the format reader supports;
- what the authoring library supports;
- what is reachable through a FileFormat argument today;
- what is reachable only through the converter or the lower-level APIs.

Collapsing those four into a single "supported" column is the specific failure
this contract exists to prevent. The authoring library will support tiled,
payload-backed output before any FileFormat argument reaches it, and a README
that hides the difference misrepresents what opening a `.tif` file actually
does.

The **Cost and limits** section is mandatory for a raster plugin and states the
interactive vertex ceiling, the default representation, and what a user should
do when a source is too large. A user who hits a limit should find the answer
in the README, not in a diagnostic they have to search for.

## Relationship to the shared documentation

A module README describes that module. It does not restate a shared contract;
it links to it. The normative sources are:

| Topic | Document |
| --- | --- |
| Structure, identities, dependency directions | [WORKSPACE.md](../architecture/WORKSPACE.md) |
| Coordinates, pixel anchoring, local origin | [COORDINATE_MODEL.md](../architecture/COORDINATE_MODEL.md) |
| Reader surface and read options | [RASTER_READER.md](../architecture/RASTER_READER.md) |
| Authored shapes | [REPRESENTATIONS.md](../architecture/REPRESENTATIONS.md) |
| Supported source data and authored USD | [CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md) |
| Authored metadata names and types | [RASTER_METADATA.md](../reference/RASTER_METADATA.md) |
| Tiling and LOD | [TILING.md](../architecture/TILING.md) |
| File-format arguments | [FILE_FORMAT_ARGUMENTS.md](../architecture/FILE_FORMAT_ARGUMENTS.md) |
| Adapter thinness | [PLUGIN_ADAPTER.md](../architecture/PLUGIN_ADAPTER.md) |
| Diagnostics | [DIAGNOSTICS.md](../architecture/DIAGNOSTICS.md) |
| Generated cache | [CACHE.md](../architecture/CACHE.md) |
| Resolver boundary | [RESOLVER_SOURCE.md](../architecture/RESOLVER_SOURCE.md) |
| Licensing and redistribution | [DISTRIBUTION.md](../guides/DISTRIBUTION.md) |

When a module README and one of those documents disagree, the shared document
wins and the README is the bug.

## Status language

Use status words that separate capability from reachability:

```text
implemented                   in this module, tested
implemented, not connected    exists in a library, no argument reaches it
planned                       has a contract, no implementation
not planned                   explicitly out of scope
```

"Not implemented" without qualification is only correct when no code in the
repository performs the behavior.

## The non-responsibilities section

This section is the most valuable one and the easiest to skip. It is where a
module states, in prose, what it will not do:

```text
## Non-responsibilities

usdGeoTiff does not author USD, perform HTTP, decide tiling policy, or create
materials. It decodes TIFF and GeoTIFF structure and returns raster windows.
```

Written down, it is a contract a reviewer can point at. Left implicit, it
erodes one convenient exception at a time.
