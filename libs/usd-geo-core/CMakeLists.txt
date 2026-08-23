cmake_minimum_required(VERSION 3.23)

project(usdGeoCore
    VERSION 0.1.0
    DESCRIPTION "Format-independent geospatial values and diagnostics"
    LANGUAGES CXX)

add_library(usdGeoCore STATIC
    src/CacheKey.cpp
    src/CrsDescription.cpp
    src/Diagnostic.cpp
    src/GeoBounds.cpp
    src/LocalOrigin.cpp
    src/TileId.cpp)
add_library(usdgeo::core ALIAS usdGeoCore)
set_target_properties(usdGeoCore PROPERTIES
    EXPORT_NAME core
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(usdGeoCore PUBLIC cxx_std_17)
target_include_directories(usdGeoCore
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

if(MSVC)
    # /utf-8: the sources are UTF-8, and MSVC otherwise reads them in the host
    #   code page, which mangles a non-ASCII literal differently on each host.
    # /EHsc: the standard containers here allocate, so bad_alloc is reachable.
    #   Without it MSVC emits no unwind information and a failed allocation
    #   terminates instead of propagating. CMake does not add it by default for
    #   every generator, so it is declared rather than assumed.
    target_compile_options(usdGeoCore PRIVATE /utf-8 /EHsc)
endif()

include(GNUInstallDirs)
install(TARGETS usdGeoCore
    EXPORT usdGeoCoreTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(USDRASTER_BUILD_TESTS)
    add_subdirectory(tests)
endif()
