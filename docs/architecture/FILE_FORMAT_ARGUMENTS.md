# File-Format Argument Contract

A file-format argument changes what a layer contains. It therefore changes
layer identity, participates in the cache key, and is validated rather than
guessed.

This contract applies to every raster bundle. The GeoTIFF bundle is the first
implementation.

## 1. Why arguments exist

Three reasons, and only these three:

1. **Selecting a representation.** One source can become metadata, a mesh, or
   an image plane. Nothing in the file says which the caller wants.
2. **Supplying what the file does not carry.** An ungeoreferenced TIFF has no
   CRS; a caller that knows the CRS must be able to state it.
3. **Bounding cost.** Sampling step and LOD profile decide whether opening a
   4 GB DEM takes 200 ms or 200 seconds.

An argument is not a place to put processing options that belong to the
converter, and not a place to put transport options that belong to the
resolver.

## 2. Syntax

Arguments use the standard `SDF_FORMAT_ARGS` encoding:

```usda
def "Terrain" (
    references = @terrain.tif:SDF_FORMAT_ARGS:representation=mesh&lod=balanced@
)
{
}
```

The same encoding works with a resolver-provided asset, because the plugin
never inspects the identifier:

```usda
def "Terrain" (
    references = @https://example.com/dem.tif:SDF_FORMAT_ARGS:representation=mesh@
)
{
}
```

Boolean arguments accept `true` / `false` only. Numeric arguments are parsed
strictly; a trailing unit, a thousands separator, or a locale-dependent decimal
separator is an error, not a best-effort parse.

## 3. Argument surface

Arguments are introduced in the milestone that implements the behavior they
reach. Nothing below is exposed before it is connected; an argument that is
parsed but ignored is forbidden.

### v0.1.0 — metadata

| Argument | Values | Default | Effect |
| --- | --- | --- | --- |
| `representation` | `metadata` | `metadata` | Selects the authored shape |

### v0.2.0 — pixel access

| Argument | Values | Default | Effect |
| --- | --- | --- | --- |
| `band` | integer `>= 1` | `1` | Selects the source band |

### v0.3.0 — mesh

| Argument | Values | Default | Effect |
| --- | --- | --- | --- |
| `representation` | `metadata`, `mesh` | `metadata` | Selects the authored shape |
| `lod` | `off`, `preview`, `balanced`, `quality` | `preview` | Maps to a sampling step; see [TILING.md](TILING.md) |
| `heightScale` | float `> 0` | `1.0` | Vertical exaggeration, recorded as lossy when not `1.0` |
| `nodata` | `skip`, `fill`, `keep` | `skip` | NoData face policy |
| `fillValue` | float | — | Required when `nodata=fill`, an error otherwise |
| `epsg` | integer | — | Supplies a CRS the file does not carry; conflicts with an in-file CRS are an error |
| `pixelAnchor` | `area`, `point` | — | Supplies anchoring when `GTRasterTypeGeoKey` is absent |

### v0.4.0 — tiling

| Argument | Values | Default | Effect |
| --- | --- | --- | --- |
| `tileSize` | integer, power of two, `>= 64` | — | Enables tiled payload authoring |

### Later

| Argument | Milestone | Effect |
| --- | --- | --- |
| `representation=image` | image milestone | Orthophoto plane authoring |
| `meshStep` | if profiles prove insufficient | Explicit sampling step, overriding `lod` |
| `overview` | COG milestone | Forces an overview level; test and diagnostic use |

`meshStep` is listed last deliberately. `lod` profiles are the primary
interface precisely so that the mapping can be retuned without breaking
existing layer identities. Publishing a raw step first would freeze it.

## 4. Rules

1. **Every argument is validated.** An unknown argument name is an error. An
   unparsable value is an error. An inapplicable combination — `fillValue`
   without `nodata=fill`, `band=3` on a single-band source — is an error. See
   [DIAGNOSTICS.md](DIAGNOSTICS.md) for the codes.
2. **No silent ignoring.** If an argument cannot take effect, the read fails.
   A user who asked for `lod=quality` and got `preview` has been misled.
3. **No guessing.** A missing CRS is reported; it is never inferred from a
   filename, a directory, or a heuristic about coordinate magnitude.
4. **Normalization happens before anything else.** Arguments are parsed and
   normalized to a canonical form — canonical spelling, canonical numeric
   formatting, defaults made explicit — before the reader is constructed and
   before a cache lookup. `lod=BALANCED` and `lod=balanced` normalize to the
   same identity; `heightScale=1` and `heightScale=1.0` do too.
5. **Normalized arguments participate in layer identity.** Two references that
   normalize identically share a layer; two that do not, do not.
6. **Parsing lives in the plugin, validation in the shared layer.** The
   readers and `usdRasterAuthoring` never see the `Sdf` argument encoding.
7. **Arguments never carry secrets.** No token, header, credential, or signed
   URL is ever an argument value. Transport authentication belongs to the
   resolver and never reaches a layer identity or a cache descriptor.
8. **Arguments never carry paths for transport.** A plugin does not accept a
   base URL, a mirror list, or a timeout.

## 5. Relationship to read options

`RasterReadOptions` is the internal contract; arguments are the external one.
They are not the same surface, and the mapping is explicit:

```text
representation  -> authoring entry point selection   (not a read option)
band            -> RasterReadOptions::band
lod             -> RasterReadOptions::samplingStep   (through the profile table)
overview        -> RasterReadOptions::overviewLevel
heightScale     -> authoring parameter               (not a read option)
nodata          -> authoring parameter
tileSize        -> tiling parameter
```

`memoryBudgetBytes` and `isCancelled` are host-supplied, not arguments: a
cancellation callback cannot be encoded in a layer identity, and a memory
budget is a property of the process, not of the data.

## 6. Dynamic file-format arguments

The bundle registers as a dynamic file format so that a compact set of
argument values can be exposed as prim metadata and so that changing them
recomposes the layer. The dynamic surface is kept narrow: `representation` and
`lod` are the intended dynamic fields, and everything else stays in
`SDF_FORMAT_ARGS`. The rationale and compatibility statement are recorded in
[ADR-0003](../adr/0003-windowed-raster-reader.md) and in
[compatibility/OPENUSD.md](../compatibility/OPENUSD.md).

## 7. Testing

For every argument:

- a valid value produces the documented change in the authored stage;
- an invalid value produces the documented diagnostic code;
- an unknown name produces the unknown-argument code;
- two normalizing-equal spellings share a layer;
- two normalizing-different values do not share a layer or a cache entry;
- an argument that alters topology never reuses an entry cached under another
  value.

The last two are the ones that fail silently in production if untested, so they
are required, not optional.
