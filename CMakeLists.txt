cmake_minimum_required(VERSION 3.23)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
     _usd_raster_version LIMIT_COUNT 1)
string(STRIP "${_usd_raster_version}" _usd_raster_version)

project(usd-raster-plugins
    VERSION ${_usd_raster_version}
    DESCRIPTION "OpenUSD file format plugins and libraries for raster geospatial data"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USDRASTER_BUILD_TESTS "Build usd-raster-plugins tests" ON)
if(USDRASTER_BUILD_TESTS)
    enable_testing()
endif()

# The core lane. USDRASTER_CORE_ONLY=ON must configure, build, and test with no
# OpenUSD available anywhere on the machine; see invariant 1 of
# docs/architecture/WORKSPACE.md. It is verified per pull request, so a core
# library that acquires an OpenUSD dependency fails CI rather than review.
option(USDRASTER_CORE_ONLY
       "Build only the OpenUSD-free core libraries" OFF)

set(_usdraster_build_usd_default OFF)
if(DEFINED pxr_ROOT OR DEFINED OpenUSD_ROOT)
    set(_usdraster_build_usd_default ON)
endif()
if(USDRASTER_CORE_ONLY)
    set(_usdraster_build_usd_default OFF)
endif()
option(USDRASTER_BUILD_USD "Build the OpenUSD authoring bridge, plugins, and tools"
       ${_usdraster_build_usd_default})
if(USDRASTER_CORE_ONLY AND USDRASTER_BUILD_USD)
    message(FATAL_ERROR
        "USDRASTER_CORE_ONLY and USDRASTER_BUILD_USD are mutually exclusive.")
endif()

option(USDRASTER_ENABLE_LIBTIFF
       "Enable the optional libtiff backend for GeoTIFF pixel decoding" OFF)
if(USDRASTER_CORE_ONLY AND USDRASTER_ENABLE_LIBTIFF)
    message(FATAL_ERROR
        "USDRASTER_CORE_ONLY and USDRASTER_ENABLE_LIBTIFF are mutually exclusive.")
endif()
if(USDRASTER_ENABLE_LIBTIFF)
    add_subdirectory(third_party)
endif()

# Modules are added as their first tested capability lands, not scaffolded
# empty; see docs/architecture/WORKSPACE.md section 1.
add_subdirectory(libs/usd-geo-core)
add_subdirectory(libs/usd-raster-core)
add_subdirectory(libs/usd-geotiff)

if(USDRASTER_BUILD_USD)
    # libs/usd-raster-authoring, plugins/raster-geotiff, and
    # tools/usd-raster-convert enter here at milestones 4, 2, and 7.
endif()

# Repository-level checks. These are Python rather than C++ because they check
# the repository, not the code: what a core header includes, and whether the
# fixture generator still produces the bytes the golden tests expect.
if(USDRASTER_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_test(NAME core_dependency_check
            COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_core_headers.py"
                    --root "${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME fixture_determinism
            COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_fixtures.py"
                    --check
                    --out "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/generated")
    else()
        # Not fatal: the C++ core lane must build on a machine with no Python.
        # The checks are required in CI, where Python is present.
        message(STATUS
            "Python3 not found; skipping the repository-level checks")
    endif()
endif()
