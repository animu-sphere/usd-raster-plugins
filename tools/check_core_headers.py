#!/usr/bin/env python3
"""Fail if an OpenUSD-free core header acquires a forbidden dependency.

Invariant 1 of docs/architecture/WORKSPACE.md says `usdGeoCore` depends on
nothing and `usdRasterCore` depends only on `usdGeoCore`. The core CI lane
catches a violation eventually -- it builds with no OpenUSD present, so an
`#include <pxr/...>` fails to compile there. This check catches it sooner and
says why, and it also catches the cases the lane cannot: a libtiff or transport
include on a machine that happens to have those headers installed would compile
in the core lane and still be a contract violation.

It is a text check, not a compile. That is the point: it runs in milliseconds
with no toolchain, so it can be a required gate everywhere.

Usage:
    python tools/check_core_headers.py [--root DIR]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# Module directory -> (its own include prefix, the prefixes it may reach).
#
# The directory name and the include prefix differ deliberately -- the external
# name uses the explicit term while the C++ prefix stays short, per section 2 of
# the workspace contract -- so the mapping is spelled out rather than derived.
# Anything outside these prefixes and the standard library is a violation, which
# means a new dependency has to be declared here, where a reviewer sees it.
ALLOWED = {
    "libs/usd-geo-core": ("usdgeo/", ()),
    "libs/usd-raster-core": ("usdraster/", ("usdgeo/",)),
}

# Named so a violation reports what it actually is rather than "not allowed".
FORBIDDEN = [
    (re.compile(r"^pxr/"), "OpenUSD"),
    (re.compile(r"^(tiff|tiffio|xtiffio|geotiff|geo_normalize|geovalues)\.h$"),
     "libtiff/libgeotiff"),
    (re.compile(r"^(proj|proj_api)\.h$"), "PROJ"),
    (re.compile(r"^(gdal|gdal_priv|ogr_spatialref|cpl_)"), "GDAL"),
    (re.compile(r"^curl/"), "libcurl (transport)"),
    (re.compile(r"^(openssl|aws|azure|google/cloud)/"), "a cloud or crypto SDK"),
]

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')

# The C++17 standard library headers a core module may use. Listing them, rather
# than accepting anything without a slash, means a stray project header in the
# wrong include form is caught too.
STDLIB = {
    "algorithm", "array", "atomic", "bitset", "cassert", "cctype", "cerrno",
    "cfenv", "cfloat", "charconv", "chrono", "cinttypes", "climits", "clocale",
    "cmath", "complex", "condition_variable", "csetjmp", "csignal", "cstdarg",
    "cstddef", "cstdint", "cstdio", "cstdlib", "cstring", "ctime", "cuchar",
    "cwchar", "cwctype", "deque", "exception", "execution", "filesystem",
    "forward_list", "fstream", "functional", "future", "initializer_list",
    "iomanip", "ios", "iosfwd", "iostream", "istream", "iterator", "limits",
    "list", "locale", "map", "memory", "memory_resource", "mutex", "new",
    "numeric", "optional", "ostream", "queue", "random", "ratio", "regex",
    "scoped_allocator", "set", "shared_mutex", "sstream", "stack",
    "stdexcept", "streambuf", "string", "string_view", "system_error",
    "thread", "tuple", "type_traits", "typeindex", "typeinfo", "unordered_map",
    "unordered_set", "utility", "valarray", "variant", "vector",
}


def check_file(path: Path, module: str, own: str, allowed: tuple) -> list:
    violations = []
    for number, line in enumerate(
            path.read_text(encoding="utf-8").splitlines(), start=1):
        match = INCLUDE_RE.match(line)
        if not match:
            continue
        include = match.group(1).replace("\\", "/")

        for pattern, name in FORBIDDEN:
            if pattern.match(include):
                violations.append(
                    (path, number, include,
                     f"{module} must not depend on {name}"))
                break
        else:
            if include in STDLIB:
                continue
            if include.startswith(own) or any(
                    include.startswith(prefix) for prefix in allowed):
                continue
            if "/" not in include and (path.parent / include).is_file():
                # A sibling header included by bare name.
                continue
            permitted = ", ".join((own,) + allowed)
            violations.append(
                (path, number, include,
                 f"{module} may include only the standard library "
                 f"and {permitted}"))
    return violations


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("."),
                        help="repository root (default: %(default)s)")
    args = parser.parse_args(argv)

    violations = []
    checked = 0
    for module, (own, allowed) in ALLOWED.items():
        directory = args.root / module
        if not directory.is_dir():
            # A module is created when its first tested capability lands, so an
            # absent one is expected, not a failure.
            continue
        # Headers and sources both: a forbidden include in a .cpp is the same
        # dependency, it is just harder to see.
        for path in sorted(directory.rglob("*")):
            if path.suffix not in (".h", ".hpp", ".cpp"):
                continue
            checked += 1
            violations.extend(check_file(path, module, own, allowed))

    if violations:
        print("forbidden includes in the OpenUSD-free core:", file=sys.stderr)
        for path, number, include, reason in violations:
            rel = path.relative_to(args.root) if args.root != Path(".") else path
            print(f"  {rel}:{number}: #include <{include}> -- {reason}",
                  file=sys.stderr)
        print("\nSee invariant 1 and section 2 of "
              "docs/architecture/WORKSPACE.md.", file=sys.stderr)
        return 1

    print(f"core dependency check: {checked} files clean")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
