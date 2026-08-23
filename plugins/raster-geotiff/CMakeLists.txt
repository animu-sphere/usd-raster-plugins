# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.22)

project(RasterGeotiffPlugin
    VERSION 0.1.0
    DESCRIPTION "OpenUSD tif file-format plugin"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/OpenStrataPlugin.cmake")
openstrata_default_build_type()

# Resolved from the runtime OpenStrata activates (toolchain sets CMAKE_PREFIX_PATH).
# Guarded so a parent project (a dual-mode root that `add_subdirectory()`s this
# bundle) can resolve OpenUSD once and have it flow down here.
if(NOT pxr_FOUND)
    find_package(pxr REQUIRED CONFIG)
endif()

if(NOT TARGET usdraster::core)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-geo-core"
                     "${CMAKE_CURRENT_BINARY_DIR}/usd-geo-core")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-raster-core"
                     "${CMAKE_CURRENT_BINARY_DIR}/usd-raster-core")
endif()
if(NOT TARGET usdgeotiff::core)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-geotiff"
                     "${CMAKE_CURRENT_BINARY_DIR}/usd-geotiff")
endif()
if(NOT TARGET usdraster::authoring)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-raster-authoring"
                     "${CMAKE_CURRENT_BINARY_DIR}/usd-raster-authoring")
endif()

set(PLUGIN_NAME UsdRasterGeoTiffFileFormat)

add_library(${PLUGIN_NAME} SHARED
    src/RasterGeotiffFileFormat.cpp)

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17)
target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/usd-raster-authoring/include"
    ${PXR_INCLUDE_DIRS})
target_link_libraries(${PLUGIN_NAME} PRIVATE
    usdgeotiff::core
    usdraster::authoring)

# `ost plugin build` writes this fragment when the bundle co-hosts a compiled
# schema (`schema.usda` + `provides: usd-schema:*`). It adds the generated typed
# API sources to this same shared library and defines the generated `*_EXPORTS`
# macro so the schema symbols are exported from the plugin binary. Plain
# file-format bundles have no fragment and stay unchanged.
if(DEFINED OPENSTRATA_SCHEMA_SOURCES_FILE AND EXISTS "${OPENSTRATA_SCHEMA_SOURCES_FILE}")
    include("${OPENSTRATA_SCHEMA_SOURCES_FILE}")
endif()

openstrata_link_openusd(
    TARGET ${PLUGIN_NAME}
    COMPONENTS arch tf gf vt sdf usd)

# The copied helper keeps output/install mechanics identical in standalone and
# add_subdirectory workspace builds. This file still owns the target, OpenUSD
# components, resource path, and fixtures as the bundle's readable policy.
openstrata_configure_plugin(
    TARGET ${PLUGIN_NAME}
    PLUG_INFO_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/raster-geotiff/plugInfo.json.in"
    PLUG_INFO_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/raster-geotiff/plugInfo.json")

openstrata_install_plugin_bundle(
    TARGET ${PLUGIN_NAME}
    RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/raster-geotiff/plugInfo.json"
    RESOURCE_DESTINATION "plugin/resources/raster-geotiff"
    MANIFESTS
        "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.plugin.yaml"
        "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.scaffold.yaml"
    FIXTURE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
