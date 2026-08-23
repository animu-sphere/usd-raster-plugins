# Installing and registering the plugin

The `raster-geotiff` bundle provides the metadata-only GeoTIFF FileFormat.
Mesh authoring and pixel decoding are later milestones.

## Bundle layout

```text
raster-geotiff/
    lib/
        UsdRasterGeoTiffFileFormat.dll     (.so / .dylib)
    plugin/
        resources/
            raster-geotiff/
                plugInfo.json
    docs/
        README.md
        DIAGNOSTICS.md
    LICENSE
    THIRD_PARTY_NOTICES.md
```

The installed shared library name and the `plugInfo.json` type name match. See
section 6 of [WORKSPACE.md](../architecture/WORKSPACE.md).

## Registering with OpenUSD

Point `PXR_PLUGINPATH_NAME` at the directory containing `plugInfo.json`:

```bash
# Linux / macOS
export PXR_PLUGINPATH_NAME=/opt/raster-geotiff/plugin/resources/raster-geotiff:$PXR_PLUGINPATH_NAME
```

```powershell
# Windows
$env:PXR_PLUGINPATH_NAME = "C:\plugins\raster-geotiff\plugin\resources\raster-geotiff;$env:PXR_PLUGINPATH_NAME"
```

The shared library must be loadable from that location — on Windows that means
the `lib` directory is on `PATH` or beside the loader, and on Linux and macOS
that `RPATH` or `LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH` resolves the OpenUSD
libraries the bundle links against.

Verify:

```bash
usdcat --flatten terrain.tif
```

If the format is not registered, OpenUSD reports an unknown file format rather
than a plugin error, which is the usual sign that `PXR_PLUGINPATH_NAME` points
at the wrong directory — it must point at the directory holding
`plugInfo.json`, not its parent.

## Adding a resolver for remote sources

Remote GeoTIFF is a property of the composed system, not a feature of this
bundle. Install an `ArResolver` that handles the scheme you need — for example
`usd-http-resolver` — and register it the same way:

```bash
export PXR_PLUGINPATH_NAME=/opt/usd-http-resolver/resources:$PXR_PLUGINPATH_NAME
```

With both registered:

```bash
usdview 'https://example.com/dem.tif:SDF_FORMAT_ARGS:representation=metadata'
```

Nothing in this bundle parses that URL. If it fails, the question is whether
the resolver resolved and opened the asset, not whether the raster plugin
understands HTTP — it does not, by design. See
[ADR-0002](../adr/0002-resolver-owns-transport.md).

## Opening a file

```bash
# metadata only, the default: cheap even for a large or remote source
usdview terrain.tif

# an elevation mesh at preview density
usdview 'terrain.tif:SDF_FORMAT_ARGS:representation=mesh&lod=preview'

# a specific band with vertical exaggeration
usdview 'terrain.tif:SDF_FORMAT_ARGS:representation=mesh&band=2&heightScale=2.0'
```

Argument syntax and the full surface are in
[FILE_FORMAT_ARGUMENTS.md](../architecture/FILE_FORMAT_ARGUMENTS.md).

## Referencing from a layer

```usda
#usda 1.0

def "Terrain" (
    references = @terrain.tif:SDF_FORMAT_ARGS:representation=mesh&lod=balanced@
)
{
}
```

## Large sources

A FileFormat read enforces an interactive vertex ceiling. When a request
exceeds it, the read fails with a message naming `usd-raster-convert` and the
arguments that would fit:

```bash
usd-raster-convert terrain.tif Terrain.usda \
  --representation mesh --lod balanced --tile-size 512
```

Then reference the generated `Terrain.usda`, whose tiles are payload-backed and
load on demand. The reasoning is in
[ADR-0008](../adr/0008-preview-vs-converter.md).

## What you get

| Opening | Result |
| --- | --- |
| `.tif` with no arguments | A `Scope` with CRS, bounds, band, and transform metadata; no pixels read |
| `representation=mesh` | A `UsdGeomMesh` elevation grid, stage-local, Y-up, one meter per unit |
| A converted `.usda` | An `Xform` hierarchy of payload-backed tiles |

Authored property names and types are fixed in
[RASTER_METADATA.md](../reference/RASTER_METADATA.md).
