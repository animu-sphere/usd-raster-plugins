"""Exercise the packaged GeoTIFF FileFormat through OpenUSD.

SPDX-License-Identifier: Apache-2.0
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

from pxr import Sdf


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=Path, required=True)
    args = parser.parse_args()
    prefix = args.prefix.resolve()
    fixture = prefix / "bundles/raster-geotiff/tests/fixtures/basic.tif"
    report = {
        "schema": 1,
        "component": "usd-raster-plugins",
        "fixture": str(fixture),
        "status": "failed",
    }
    try:
        file_format = Sdf.FileFormat.FindByExtension("tif")
        if not file_format or file_format.formatId != "tif":
            raise RuntimeError("raster-geotiff is not registered for .tif")
        layer = Sdf.Layer.FindOrOpen(str(fixture))
        if not layer:
            raise RuntimeError("packaged GeoTIFF fixture did not open")
        authored = layer.ExportToString()
        required = [
            'def Scope "Raster"',
            "uint64 raster:width = 2",
            "uint64 raster:height = 2",
            'string geo:crs = "EPSG:32654"',
            "double[] raster:geoTransform",
        ]
        missing = [value for value in required if value not in authored]
        if missing:
            raise RuntimeError(f"authored layer is missing {missing}")
        report.update({
            "status": "passed",
            "formatId": file_format.formatId,
            "layerIdentifier": layer.identifier,
            "authoredLayerDigest": "sha256:" + hashlib.sha256(authored.encode()).hexdigest(),
            "observations": {
                "registered": True,
                "opened": True,
                "width": 2,
                "height": 2,
                "crs": "EPSG:32654",
            },
        })
    except Exception as error:
        report["error"] = str(error)
    print(json.dumps(report, indent=2))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    sys.exit(main())

