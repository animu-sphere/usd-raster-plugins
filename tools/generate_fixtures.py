#!/usr/bin/env python3
"""Generate the synthetic GeoTIFF fixtures the golden tests read.

Fixtures are checked in as code, not as binaries. The generator writes TIFF
bytes directly rather than through GDAL or libtiff, for three reasons:

  * ADR-0007 keeps GDAL out of the build, and a fixture generator that needs it
    puts it back in through the test lane;
  * a writer that emits exactly the tag layout a case is about is what makes a
    negative fixture -- a missing GTRasterTypeGeoKey, a conflicting
    georeferencing pair -- expressible at all;
  * the output must be byte-identical on Windows, Linux, and macOS, which a
    third-party writer does not promise across its own versions.

Determinism: every value written is an explicit literal or a closed-form
function of the pixel index. Nothing reads the clock, the filesystem, the
locale, or a hash seed. `--check` re-derives the bytes and compares them
against MANIFEST.sha256 without touching the output files.

Usage:
    python tools/generate_fixtures.py [--out DIR] [--check] [--list]
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

# --- TIFF field types -------------------------------------------------------

BYTE = 1
ASCII = 2
SHORT = 3
LONG = 4
RATIONAL = 5
FLOAT = 11
DOUBLE = 12
LONG8 = 16

_TYPE_CODE = {BYTE: "B", SHORT: "H", LONG: "I", FLOAT: "f", DOUBLE: "d",
              LONG8: "Q"}

# --- TIFF tags --------------------------------------------------------------

IMAGE_WIDTH = 256
IMAGE_LENGTH = 257
BITS_PER_SAMPLE = 258
COMPRESSION = 259
PHOTOMETRIC = 262
STRIP_OFFSETS = 273
SAMPLES_PER_PIXEL = 277
ROWS_PER_STRIP = 278
STRIP_BYTE_COUNTS = 279
PLANAR_CONFIG = 284
TILE_WIDTH = 322
TILE_LENGTH = 323
TILE_OFFSETS = 324
TILE_BYTE_COUNTS = 325
SAMPLE_FORMAT = 339
MODEL_PIXEL_SCALE = 33550
MODEL_TIEPOINT = 33922
MODEL_TRANSFORMATION = 34264
GEO_KEY_DIRECTORY = 34735
GEO_DOUBLE_PARAMS = 34736
GEO_ASCII_PARAMS = 34737
GDAL_METADATA = 42112
GDAL_NODATA = 42113

# --- GeoTIFF keys -----------------------------------------------------------

GT_MODEL_TYPE = 1024          # 1 projected, 2 geographic
GT_RASTER_TYPE = 1025         # 1 PixelIsArea, 2 PixelIsPoint
GT_CITATION = 1026
GEOGRAPHIC_TYPE = 2048
PROJECTED_CS_TYPE = 3072
PROJ_LINEAR_UNITS = 3076      # 9001 metre

MODEL_TYPE_PROJECTED = 1
MODEL_TYPE_GEOGRAPHIC = 2
RASTER_TYPE_PIXEL_IS_AREA = 1
RASTER_TYPE_PIXEL_IS_POINT = 2
LINEAR_UNITS_METRE = 9001

# --- Sample formats ---------------------------------------------------------

SAMPLE_FORMAT_UINT = 1
SAMPLE_FORMAT_INT = 2
SAMPLE_FORMAT_IEEEFP = 3

_SAMPLE = {
    "uint8": (SAMPLE_FORMAT_UINT, 8, "B"),
    "uint16": (SAMPLE_FORMAT_UINT, 16, "H"),
    "int16": (SAMPLE_FORMAT_INT, 16, "h"),
    "uint32": (SAMPLE_FORMAT_UINT, 32, "I"),
    "float32": (SAMPLE_FORMAT_IEEEFP, 32, "f"),
    "float64": (SAMPLE_FORMAT_IEEEFP, 64, "d"),
}


class TiffWriter:
    """A minimal, deterministic TIFF/BigTIFF writer.

    Emits one IFD, uncompressed, single plane. Layout is fixed: header, IFD,
    out-of-line tag values in ascending tag order, then pixel data. Nothing in
    the layout depends on iteration order that Python does not guarantee.
    """

    def __init__(self, big_endian=False, bigtiff=False):
        self.endian = ">" if big_endian else "<"
        self.big_endian = big_endian
        self.bigtiff = bigtiff
        self._entries = []

    # -- entry construction --

    def add(self, tag, field_type, values):
        if field_type == ASCII:
            payload = values.encode("ascii") + b"\0"
            count = len(payload)
        else:
            if not isinstance(values, (list, tuple)):
                values = [values]
            count = len(values)
            payload = struct.pack(
                self.endian + str(count) + _TYPE_CODE[field_type], *values)
        self._entries.append((tag, field_type, count, payload))

    # -- layout --
    #
    # Strip and tile offsets are only known once the pixel data is placed,
    # which is after the IFD is sized. The two-pass shape below sizes
    # everything first and then writes, so an offset is never guessed and
    # patched afterwards.

    def _offset_code(self):
        return "Q" if self.bigtiff else "I"

    def _inline_capacity(self):
        return 8 if self.bigtiff else 4

    def _header_size(self):
        return 16 if self.bigtiff else 8

    def _entry_size(self):
        return 20 if self.bigtiff else 12

    def _ifd_size(self):
        count_size = 8 if self.bigtiff else 2
        next_size = 8 if self.bigtiff else 4
        return count_size + self._entry_size() * len(self._entries) + next_size

    def build(self, pixel_data, offset_tag):
        """Serialize. `offset_tag` -- StripOffsets or TileOffsets -- is
        rewritten with the resolved pixel-data offsets; its placeholder count
        fixes how many equal-sized segments the pixel data is split into."""
        entries = sorted(self._entries, key=lambda entry: entry[0])

        ifd_offset = self._header_size()
        cursor = ifd_offset + self._ifd_size()

        placed = []
        for tag, ftype, count, payload in entries:
            if len(payload) <= self._inline_capacity():
                placed.append((tag, ftype, count, payload, -1))
            else:
                placed.append((tag, ftype, count, payload, cursor))
                cursor += len(payload)
                if cursor % 2:  # TIFF values start on a word boundary.
                    cursor += 1

        pixel_offset = cursor

        resolved = []
        for tag, ftype, count, payload, ext in placed:
            if tag == offset_tag:
                segment_size = len(pixel_data) // count
                offsets = [pixel_offset + i * segment_size
                           for i in range(count)]
                new_payload = struct.pack(
                    self.endian + str(count) + _TYPE_CODE[ftype], *offsets)
                assert len(new_payload) == len(payload), \
                    "offset payload changed size; the layout would shift"
                payload = new_payload
            resolved.append((tag, ftype, count, payload, ext))

        out = bytearray()
        out += self._pack_header(ifd_offset)
        out += self._pack_ifd(resolved)
        for _tag, _ftype, _count, payload, ext in resolved:
            if ext < 0:
                continue
            assert len(out) == ext, "external value placement drifted"
            out += payload
            if len(out) % 2:
                out += b"\0"
        assert len(out) == pixel_offset, "pixel placement drifted"
        out += pixel_data
        return bytes(out)

    def _pack_header(self, ifd_offset):
        order = b"MM" if self.big_endian else b"II"
        if self.bigtiff:
            return order + struct.pack(self.endian + "HHHQ", 43, 8, 0,
                                       ifd_offset)
        return order + struct.pack(self.endian + "HI", 42, ifd_offset)

    def _pack_ifd(self, entries):
        out = bytearray()
        if self.bigtiff:
            out += struct.pack(self.endian + "Q", len(entries))
        else:
            out += struct.pack(self.endian + "H", len(entries))
        capacity = self._inline_capacity()
        for tag, ftype, count, payload, ext in entries:
            if self.bigtiff:
                out += struct.pack(self.endian + "HHQ", tag, ftype, count)
            else:
                out += struct.pack(self.endian + "HHI", tag, ftype, count)
            if ext < 0:
                # An inline value is left-justified in the field, in both byte
                # orders. The remaining bytes are zero, not arbitrary.
                out += payload + b"\0" * (capacity - len(payload))
            else:
                out += struct.pack(self.endian + self._offset_code(), ext)
        out += struct.pack(self.endian + self._offset_code(), 0)
        return bytes(out)


def geo_key_directory(keys):
    """Pack GeoTIFF keys. Each key is (id, tag_location, count, value); a
    tag_location of 0 means the value is the short itself."""
    directory = [1, 1, 1, len(keys)]
    for key_id, location, count, value in sorted(keys):
        directory += [key_id, location, count, value]
    return directory


def encode_samples(values, sample_type, endian):
    _fmt, _bits, code = _SAMPLE[sample_type]
    return struct.pack(endian + str(len(values)) + code, *values)


def base_image_entries(writer, width, height, sample_type):
    fmt, bits, _code = _SAMPLE[sample_type]
    writer.add(IMAGE_WIDTH, LONG, width)
    writer.add(IMAGE_LENGTH, LONG, height)
    writer.add(BITS_PER_SAMPLE, SHORT, bits)
    writer.add(COMPRESSION, SHORT, 1)          # none
    writer.add(PHOTOMETRIC, SHORT, 1)          # BlackIsZero
    writer.add(SAMPLES_PER_PIXEL, SHORT, 1)
    writer.add(PLANAR_CONFIG, SHORT, 1)        # chunky
    writer.add(SAMPLE_FORMAT, SHORT, fmt)


def striped(writer, width, height, sample_type, rows_per_strip, values):
    _fmt, bits, _code = _SAMPLE[sample_type]
    assert height % rows_per_strip == 0, \
        "fixtures keep strips uniform so segment sizes stay exact"
    strip_count = height // rows_per_strip
    strip_bytes = width * rows_per_strip * bits // 8
    base_image_entries(writer, width, height, sample_type)
    writer.add(ROWS_PER_STRIP, LONG, rows_per_strip)
    writer.add(STRIP_OFFSETS, LONG, [0] * strip_count)
    writer.add(STRIP_BYTE_COUNTS, LONG, [strip_bytes] * strip_count)
    return encode_samples(values, sample_type, writer.endian)


def tiled(writer, width, height, sample_type, tile_width, tile_height, values):
    """Tile a row-major image. TIFF pads partial tiles; the padding here is
    zero, which is what a reader must learn to drop rather than return."""
    _fmt, bits, _code = _SAMPLE[sample_type]
    assert tile_width % 16 == 0 and tile_height % 16 == 0, \
        "TIFF requires tile dimensions to be multiples of 16"
    across = (width + tile_width - 1) // tile_width
    down = (height + tile_height - 1) // tile_height
    tile_bytes = tile_width * tile_height * bits // 8

    base_image_entries(writer, width, height, sample_type)
    writer.add(TILE_WIDTH, LONG, tile_width)
    writer.add(TILE_LENGTH, LONG, tile_height)
    writer.add(TILE_OFFSETS, LONG, [0] * (across * down))
    writer.add(TILE_BYTE_COUNTS, LONG, [tile_bytes] * (across * down))

    payload = bytearray()
    for ty in range(down):
        for tx in range(across):
            tile = []
            for row in range(tile_height):
                y = ty * tile_height + row
                for col in range(tile_width):
                    x = tx * tile_width + col
                    inside = x < width and y < height
                    tile.append(values[y * width + x] if inside else 0)
            payload += encode_samples(tile, sample_type, writer.endian)
    return bytes(payload)


# --- Georeferencing ---------------------------------------------------------

# UTM zone 54N. Chosen because an easting near 3e5 and a northing near 4.4e6
# are large enough that float32 stage-local positions would lose metre
# precision without the local-origin transform; that is the point of ADR-0006,
# and a fixture with small coordinates would never exercise it.
EPSG_UTM54N = 32654
EPSG_WGS84 = 4326

ORIGIN_EASTING = 300000.0
ORIGIN_NORTHING = 4400000.0


def projected_keys(raster_type=RASTER_TYPE_PIXEL_IS_AREA):
    keys = [(GT_MODEL_TYPE, 0, 1, MODEL_TYPE_PROJECTED),
            (PROJECTED_CS_TYPE, 0, 1, EPSG_UTM54N),
            (PROJ_LINEAR_UNITS, 0, 1, LINEAR_UNITS_METRE)]
    if raster_type is not None:
        keys.append((GT_RASTER_TYPE, 0, 1, raster_type))
    return keys


def geographic_keys():
    return [(GT_MODEL_TYPE, 0, 1, MODEL_TYPE_GEOGRAPHIC),
            (GT_RASTER_TYPE, 0, 1, RASTER_TYPE_PIXEL_IS_AREA),
            (GEOGRAPHIC_TYPE, 0, 1, EPSG_WGS84)]


def add_geo_keys(writer, keys):
    writer.add(GEO_KEY_DIRECTORY, SHORT, geo_key_directory(keys))


def add_north_up(writer, pixel_size, easting=ORIGIN_EASTING,
                 northing=ORIGIN_NORTHING):
    """North-up, expressed the way TIFF expresses it: a positive
    ModelPixelScale in y, with the downward row direction left implicit."""
    writer.add(MODEL_PIXEL_SCALE, DOUBLE, [pixel_size, pixel_size, 0.0])
    writer.add(MODEL_TIEPOINT, DOUBLE,
               [0.0, 0.0, 0.0, easting, northing, 0.0])


# --- Fixture definitions ----------------------------------------------------
#
# Elevation values are exact in both float32 and float64, so a golden test can
# compare them without a tolerance, and a mismatch then means a real bug rather
# than an accumulated rounding difference.

ELEV_2X2 = [10.0, 20.0, 30.0, 40.0]

# The positive quiet NaN, 0x7FC00000, derived from its bits rather than from
# float("nan"). CPython's float("nan") happens to produce this pattern on every
# platform the project builds on, but nothing in the language promises it, and
# a fixture whose bytes depend on that would break the byte-identical-output
# criterion in a way that reproduces only on the platform that broke it.
QUIET_NAN_F32 = struct.unpack("<f", struct.pack("<I", 0x7FC00000))[0]


def fixture_2x2_float32(big_endian=False, bigtiff=False,
                        raster_type=RASTER_TYPE_PIXEL_IS_AREA):
    writer = TiffWriter(big_endian=big_endian, bigtiff=bigtiff)
    pixels = striped(writer, 2, 2, "float32", 2, ELEV_2X2)
    add_north_up(writer, 2.0)
    add_geo_keys(writer, projected_keys(raster_type))
    return writer.build(pixels, STRIP_OFFSETS)


def fixture_2x2_rotated():
    """ModelTransformation instead of scale and tiepoint: a 30-degree rotation
    about the tiepoint, which is the case a north-up-only reader gets wrong
    silently rather than loudly."""
    writer = TiffWriter()
    pixels = striped(writer, 2, 2, "float32", 2, ELEV_2X2)
    # cos(30) and sin(30) as literals, so the fixture does not depend on the
    # platform's trigonometric library.
    cos30 = 0.8660254037844387
    sin30 = 0.5
    matrix = [2.0 * cos30, -2.0 * sin30, 0.0, ORIGIN_EASTING,
              2.0 * sin30, 2.0 * cos30, 0.0, ORIGIN_NORTHING,
              0.0, 0.0, 0.0, 0.0,
              0.0, 0.0, 0.0, 1.0]
    writer.add(MODEL_TRANSFORMATION, DOUBLE, matrix)
    add_geo_keys(writer, projected_keys())
    return writer.build(pixels, STRIP_OFFSETS)


def fixture_2x2_conflicting_georeferencing():
    """Both ModelTransformation and ModelPixelScale/ModelTiepoint, disagreeing.
    The coordinate contract fixes the precedence; this fixture is what proves
    the reader applies it rather than taking whichever tag it read first."""
    writer = TiffWriter()
    pixels = striped(writer, 2, 2, "float32", 2, ELEV_2X2)
    add_north_up(writer, 2.0)
    writer.add(MODEL_TRANSFORMATION, DOUBLE,
               [5.0, 0.0, 0.0, 1000.0,
                0.0, -5.0, 0.0, 2000.0,
                0.0, 0.0, 0.0, 0.0,
                0.0, 0.0, 0.0, 1.0])
    add_geo_keys(writer, projected_keys())
    return writer.build(pixels, STRIP_OFFSETS)


def fixture_2x2_no_georeferencing():
    """A plain TIFF. The bundle claims .tif, so this is the ordinary case of a
    non-geospatial file arriving, not an exotic error case."""
    writer = TiffWriter()
    pixels = striped(writer, 2, 2, "float32", 2, ELEV_2X2)
    return writer.build(pixels, STRIP_OFFSETS)


def fixture_2x2_no_raster_type():
    """Geo keys present, GTRasterTypeGeoKey absent. Pixel anchoring is never
    guessed, so this must warn and require the `pixelAnchor` argument."""
    writer = TiffWriter()
    pixels = striped(writer, 2, 2, "float32", 2, ELEV_2X2)
    add_north_up(writer, 2.0)
    add_geo_keys(writer, projected_keys(raster_type=None))
    return writer.build(pixels, STRIP_OFFSETS)


def fixture_2x2_nodata():
    """GDAL_NODATA is ASCII, not a typed value, which is why the reader parses
    it rather than reads it. -9999 is the conventional DEM sentinel."""
    writer = TiffWriter()
    pixels = striped(writer, 2, 2, "float32", 2, [10.0, -9999.0, 30.0, 40.0])
    add_north_up(writer, 2.0)
    add_geo_keys(writer, projected_keys())
    writer.add(GDAL_NODATA, ASCII, "-9999")
    return writer.build(pixels, STRIP_OFFSETS)


def fixture_2x2_nodata_nan():
    """NoData as NaN. Comparison is bit equality for NaN and never a tolerance;
    see docs/roadmap/phase-1-raster-core.md."""
    writer = TiffWriter()
    pixels = striped(writer, 2, 2, "float32", 2,
                     [10.0, QUIET_NAN_F32, 30.0, 40.0])
    add_north_up(writer, 2.0)
    add_geo_keys(writer, projected_keys())
    writer.add(GDAL_NODATA, ASCII, "nan")
    return writer.build(pixels, STRIP_OFFSETS)


def fixture_2x2_invalid_nodata():
    writer = TiffWriter()
    pixels = striped(writer, 2, 2, "float32", 2, ELEV_2X2)
    add_north_up(writer, 2.0)
    add_geo_keys(writer, projected_keys())
    writer.add(GDAL_NODATA, ASCII, "not-a-number-value")
    return writer.build(pixels, STRIP_OFFSETS)


def fixture_2x2_scaled_metadata():
    writer = TiffWriter()
    pixels = striped(writer, 2, 2, "uint16", 2, [10, 20, 30, 40])
    add_north_up(writer, 2.0)
    add_geo_keys(writer, projected_keys())
    writer.add(GDAL_METADATA, ASCII,
               '<GDALMetadata><Item name="DESCRIPTION">dataset title</Item>'
               '<Item name="DESCRIPTION" sample="0">'
               'Elevation &amp; height</Item><Item name="UNITTYPE" sample="0">'
               'metre</Item><Item name="scale" sample="0">\n 0.5 \n</Item>'
               '<Item name="offset" sample="0">\n 100 \n</Item></GDALMetadata>')
    return writer.build(pixels, STRIP_OFFSETS)


def fixture_2x2_default_tags():
    """A valid baseline TIFF using defaults for optional sample tags."""
    writer = TiffWriter()
    writer.add(IMAGE_WIDTH, LONG, 2)
    writer.add(IMAGE_LENGTH, LONG, 2)
    writer.add(BITS_PER_SAMPLE, SHORT, 16)
    writer.add(PHOTOMETRIC, SHORT, 1)
    writer.add(ROWS_PER_STRIP, LONG, 2)
    writer.add(STRIP_OFFSETS, LONG, [0])
    writer.add(STRIP_BYTE_COUNTS, LONG, [8])
    add_north_up(writer, 2.0)
    add_geo_keys(writer, projected_keys())
    return writer.build(encode_samples([10, 20, 30, 40], "uint16", writer.endian), STRIP_OFFSETS)


def fixture_2x2_pixel_is_point():
    return fixture_2x2_float32(raster_type=RASTER_TYPE_PIXEL_IS_POINT)


def fixture_2x2_geographic():
    """A geographic CRS reaching a metric stage. Degrees are not metres, and
    the handling of that is an open question owned by milestone 4."""
    writer = TiffWriter()
    pixels = striped(writer, 2, 2, "float64", 2, ELEV_2X2)
    writer.add(MODEL_PIXEL_SCALE, DOUBLE, [0.25, 0.25, 0.0])
    writer.add(MODEL_TIEPOINT, DOUBLE, [0.0, 0.0, 0.0, 139.0, 36.0, 0.0])
    add_geo_keys(writer, geographic_keys())
    return writer.build(pixels, STRIP_OFFSETS)


def _ramp(width, height):
    """A row-major ramp. Every value identifies its own pixel, so a window read
    that returns the wrong region fails on the values, not only on a count."""
    return [y * width + x for y in range(height) for x in range(width)]


def fixture_8x8_uint16_striped():
    """Four strips of two rows. A window crossing a strip boundary is the case
    that must report I/O amplification rather than hide it."""
    writer = TiffWriter()
    pixels = striped(writer, 8, 8, "uint16", 2, _ramp(8, 8))
    add_north_up(writer, 1.0)
    add_geo_keys(writer, projected_keys())
    return writer.build(pixels, STRIP_OFFSETS)


def fixture_32x32_uint16_tiled():
    """Four 16x16 tiles. A window inside one tile must touch only that tile,
    which the instrumented source asserts."""
    writer = TiffWriter()
    pixels = tiled(writer, 32, 32, "uint16", 16, 16, _ramp(32, 32))
    add_north_up(writer, 1.0)
    add_geo_keys(writer, projected_keys())
    return writer.build(pixels, TILE_OFFSETS)


def fixture_20x20_uint16_tiled_partial():
    """Tiles that do not divide the image evenly: 20x20 in 16x16 tiles leaves a
    four-pixel remainder on both edges, and the padding must not reach the
    caller."""
    writer = TiffWriter()
    pixels = tiled(writer, 20, 20, "uint16", 16, 16, _ramp(20, 20))
    add_north_up(writer, 1.0)
    add_geo_keys(writer, projected_keys())
    return writer.build(pixels, TILE_OFFSETS)


FIXTURES = {
    # The vertical-slice fixture: docs/roadmap/geotiff-vertical-slice.md.
    "geotiff-2x2-float32-le.tif": fixture_2x2_float32,
    "geotiff-2x2-float32-be.tif": lambda: fixture_2x2_float32(big_endian=True),
    "geotiff-2x2-float32-bigtiff.tif":
        lambda: fixture_2x2_float32(bigtiff=True),
    "geotiff-2x2-float32-pixelispoint.tif": fixture_2x2_pixel_is_point,
    "geotiff-2x2-float32-rotated.tif": fixture_2x2_rotated,
    "geotiff-2x2-float32-conflicting-geo.tif":
        fixture_2x2_conflicting_georeferencing,
    "geotiff-2x2-float32-no-geo.tif": fixture_2x2_no_georeferencing,
    "geotiff-2x2-float32-no-raster-type.tif": fixture_2x2_no_raster_type,
    "geotiff-2x2-float32-nodata.tif": fixture_2x2_nodata,
    "geotiff-2x2-float32-nodata-nan.tif": fixture_2x2_nodata_nan,
    "geotiff-2x2-float32-invalid-nodata.tif": fixture_2x2_invalid_nodata,
    "geotiff-2x2-uint16-scaled-metadata.tif": fixture_2x2_scaled_metadata,
    "geotiff-2x2-uint16-default-tags.tif": fixture_2x2_default_tags,
    "geotiff-2x2-float64-geographic.tif": fixture_2x2_geographic,
    "geotiff-8x8-uint16-striped.tif": fixture_8x8_uint16_striped,
    "geotiff-32x32-uint16-tiled.tif": fixture_32x32_uint16_tiled,
    "geotiff-20x20-uint16-tiled-partial.tif":
        fixture_20x20_uint16_tiled_partial,
}

MANIFEST_NAME = "MANIFEST.sha256"
DEFAULT_OUT = Path("tests/fixtures/generated")


def build_all():
    return {name: builder() for name, builder in sorted(FIXTURES.items())}


def manifest_text(built):
    lines = [hashlib.sha256(data).hexdigest() + "  " + name
             for name, data in sorted(built.items())]
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT,
                        help="output directory (default: %(default)s)")
    parser.add_argument("--check", action="store_true",
                        help="verify MANIFEST.sha256 without writing fixtures")
    parser.add_argument("--list", action="store_true",
                        help="list fixture names and exit")
    args = parser.parse_args(argv)

    if args.list:
        for name in sorted(FIXTURES):
            print(name)
        return 0

    built = build_all()

    if args.check:
        manifest = args.out / MANIFEST_NAME
        if not manifest.is_file():
            print("missing " + str(manifest) + "; run without --check first",
                  file=sys.stderr)
            return 1
        expected = manifest.read_text(encoding="utf-8")
        actual = manifest_text(built)
        if expected != actual:
            print("fixture bytes do not match the manifest", file=sys.stderr)
            expected_map = {}
            for line in expected.splitlines():
                digest, _, name = line.partition("  ")
                expected_map[name] = digest
            for name, data in sorted(built.items()):
                digest = hashlib.sha256(data).hexdigest()
                if expected_map.get(name) != digest:
                    print("  " + name + ": expected "
                          + str(expected_map.get(name)) + ", got " + digest,
                          file=sys.stderr)
            return 1
        print(str(len(built)) + " fixtures match " + str(manifest))
        return 0

    args.out.mkdir(parents=True, exist_ok=True)
    for name, data in sorted(built.items()):
        (args.out / name).write_bytes(data)
    (args.out / MANIFEST_NAME).write_text(manifest_text(built),
                                          encoding="utf-8", newline="\n")
    print("wrote " + str(len(built)) + " fixtures to " + str(args.out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
