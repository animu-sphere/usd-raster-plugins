# libtiff adapter

This directory owns the CMake boundary for the GeoTIFF decoding backend. The
public target is `usd-raster::libtiff`; consumers do not depend directly on a
package-specific target name.

The current provider is a system-installed libtiff selected with
`USDRASTER_USE_SYSTEM_LIBTIFF=ON`. The option is opt-in from the repository
root through `USDRASTER_ENABLE_LIBTIFF=ON`, so the OpenUSD-free core lane never
discovers or links a codec.

When a reproducible binary distribution requires vendoring, add the pinned
libtiff source and its license here, keep the same `usd-raster::libtiff`
target, and record the exact version, source digest, and transitive notices in
`THIRD_PARTY_NOTICES.md` before enabling it for packaging. Do not make the
GeoTIFF reader depend on libtiff headers through its public API.