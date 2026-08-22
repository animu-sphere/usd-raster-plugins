# FileFormat Plugin Adapter Contract

A raster FileFormat Plugin is an adapter. It connects an OpenUSD `SdfFileFormat`
entry point to libraries that know nothing about OpenUSD, and it does nothing
else.

This contract exists because the failure mode is predictable: parsing logic
that starts in a plugin is untestable without an OpenUSD runtime, unreachable
from the converter, and duplicated into every later bundle.

## 1. Rule

A plugin does exactly six things:

```text
1. register the format and its extensions
2. normalize and validate file-format arguments
3. adapt the OpenUSD asset into a project-owned RandomAccessSource
4. construct the format reader and drive it
5. call one shared authoring entry point
6. project typed diagnostics into TF_ errors and warnings
```

Anything else belongs in a library.

## 2. Forbidden in a plugin

```text
TIFF header or IFD parsing
compression handling
geotransform arithmetic
CRS interpretation
NoData policy
sampling or resampling
window or tile planning
mesh topology construction
payload layout decisions
cache key construction
HTTP, retries, URL parsing, scheme inspection
```

Each of these has an owning library. A plugin that reimplements one has moved
a contract out of a tested, OpenUSD-free module into a place where only an
integration test can reach it.

## 3. Shape

```cpp
bool UsdRasterGeoTiffFileFormat::Read(
    SdfLayer* layer,
    const std::string& resolvedPath,
    bool metadataOnly) const
{
    // 1. normalize arguments (plugin)
    RasterArguments args;
    Diagnostic diag;
    if (!ParseRasterArguments(layer->GetFileFormatArguments(), &args, &diag)) {
        return ReportAndFail(diag);
    }

    // 2. adapt the asset (plugin: the only OpenUSD-aware source)
    auto source = OpenArAssetSource(resolvedPath, &diag);
    if (!source) {
        return ReportAndFail(diag);
    }

    // 3. drive the reader (library)
    usdgeotiff::GeoTiffReader reader(std::move(source));
    // ...

    // 4. one authoring call (library)
    return usdraster::AuthorRasterLayer(layer, reader, args, &diag)
               ? true
               : ReportAndFail(diag);
}
```

The body is short by construction. When it stops being short, something has
moved into it that belongs elsewhere.

## 4. The `ArAsset` adapter

The adapter is the only place in the workspace where OpenUSD asset resolution
meets the reader:

```text
ArResolver::OpenAsset(resolvedPath)
      -> ArAsset
      -> ArAssetSource : public RandomAccessSource
      -> GeoTiffReader
```

Requirements:

- Deterministic offset reads through `ArAsset::Read(buffer, count, offset)`.
- Source size from `ArAsset::GetSize()`.
- Short reads and EOF reported as typed results, never as exceptions and never
  as silent zero-fill.
- No retries. A retry is transport policy.
- No inspection of the identifier. The adapter does not branch on `http://`,
  `file://`, or any other scheme; if it did, the plugin would be encoding
  transport knowledge.

Local files use the same interface. Remote reads must not be a parallel code
path, or they will be a permanently second-class one. See
[RESOLVER_SOURCE.md](RESOLVER_SOURCE.md).

## 5. `metadataOnly`

OpenUSD passes `metadataOnly` for cheap layer inspection. The plugin maps it
onto the metadata representation regardless of the `representation` argument,
and reads no pixels. This is the one case where a requested representation is
not honored, and it is honored again on the subsequent full read, so nothing is
silently lost.

## 6. Diagnostics projection

The reader and the authoring library return typed diagnostics. The plugin
converts them into OpenUSD diagnostics with its stable code prefix:

```text
[GTIF004] Unable to read terrain.tif: unsupported compression (code 34887)
```

The plugin owns its code table. It does not invent codes for conditions the
library already classified; it maps them. See
[DIAGNOSTICS.md](DIAGNOSTICS.md).

## 7. Cost ceiling

A FileFormat read is an interactive operation. The plugin enforces a ceiling on
what a single read may do — a maximum authored vertex count for the mesh
representation — and when a request exceeds it, it fails with a diagnostic that
names `usd-raster-convert` and the arguments that would make the request fit.

Failing with an actionable message is better than an apparently-hung host. The
converter has no such ceiling; that asymmetry is the point of
[ADR-0008](../adr/0008-preview-vs-converter.md).

## 8. Tests

Required for every bundle:

- **Discovery** — the format is registered, claims its extensions, and is found
  through `plugInfo.json` in an installed layout.
- **Stage open** — a fixture opens to a valid stage for every representation.
- **Argument matrix** — each documented argument, valid and invalid.
- **Equivalence** — the same fixture read through a local file, an in-memory
  `ArAsset`, and a resolver-provided `ArAsset` produces identical authored
  output.
- **Thinness** — the adapter source stays within its documented
  responsibilities. Reviewed, not automated; a growing plugin source file is
  the signal.
