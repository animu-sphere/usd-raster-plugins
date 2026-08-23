cmake_minimum_required(VERSION 3.23)

project(usdRasterCore
    VERSION 0.1.0
    DESCRIPTION "Format-independent raster value model and byte-source contract"
    LANGUAGES CXX)

add_library(usdRasterCore STATIC
    src/NoData.cpp
    src/RandomAccessSource.cpp
    src/RasterGeoTransform.cpp
    src/RasterGrid.cpp
    src/RasterMetadata.cpp
    src/RasterReadOptions.cpp
    src/RasterTypes.cpp
    src/RasterWindow.cpp)
add_library(usdraster::core ALIAS usdRasterCore)
set_target_properties(usdRasterCore PROPERTIES
    EXPORT_NAME core
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdRasterCore PUBLIC cxx_std_17)
target_include_directories(usdRasterCore
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

# The only allowed dependency. usdRasterCore must never acquire OpenUSD, a
# TIFF/GeoTIFF type, libtiff, or a transport library; see section 2 of
# docs/architecture/WORKSPACE.md.
target_link_libraries(usdRasterCore PUBLIC usdgeo::core)

if(MSVC)
    # /utf-8: the sources are UTF-8, and MSVC otherwise reads them in the host
    #   code page, which mangles a non-ASCII literal differently on each host.
    # /EHsc: the standard containers here allocate, so bad_alloc is reachable.
    #   Without it MSVC emits no unwind information and a failed allocation
    #   terminates instead of propagating. CMake does not add it by default for
    #   every generator, so it is declared rather than assumed.
    target_compile_options(usdRasterCore PRIVATE /utf-8 /EHsc)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS usdRasterCore
    EXPORT usdRasterCoreTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# The export set above only records what to export; without the three steps
# below nothing writes a single .cmake file, and `find_package(usdRasterCore)` --
# the package name section 2 of docs/architecture/WORKSPACE.md and
# openstrata.library.yaml both declare -- cannot resolve. The namespace here
# must match the ALIAS above, so a consumer links `usdraster::core`
# identically whether it came from the build tree or an install.
set(_usdRasterCore_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/usdRasterCore")

install(EXPORT usdRasterCoreTargets
    FILE usdRasterCoreTargets.cmake
    NAMESPACE usdraster::
    DESTINATION "${_usdRasterCore_cmakedir}")

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/usdRasterCoreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/usdRasterCoreConfig.cmake"
    INSTALL_DESTINATION "${_usdRasterCore_cmakedir}")

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/usdRasterCoreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/usdRasterCoreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/usdRasterCoreConfigVersion.cmake"
    DESTINATION "${_usdRasterCore_cmakedir}")

if(USDRASTER_BUILD_TESTS)
    add_subdirectory(tests)
endif()
