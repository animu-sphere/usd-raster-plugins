cmake_minimum_required(VERSION 3.23)

project(usdGeoTiff
    VERSION 0.2.0
    DESCRIPTION "OpenUSD-free GeoTIFF metadata reader"
    LANGUAGES CXX)

add_library(usdGeoTiff STATIC src/GeoTiffReader.cpp)
add_library(usdgeotiff::core ALIAS usdGeoTiff)
set_target_properties(usdGeoTiff PROPERTIES EXPORT_NAME core POSITION_INDEPENDENT_CODE ON)
target_compile_features(usdGeoTiff PUBLIC cxx_std_17)
target_include_directories(usdGeoTiff PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")
target_link_libraries(usdGeoTiff PUBLIC usdraster::core)
if(TARGET usd-raster::libtiff)
    target_link_libraries(usdGeoTiff PRIVATE usd-raster::libtiff)
    target_compile_definitions(usdGeoTiff PRIVATE USDRASTER_HAS_LIBTIFF=1)
endif()

if(MSVC)
    target_compile_options(usdGeoTiff PRIVATE /utf-8 /EHsc)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS usdGeoTiff EXPORT usdGeoTiffTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
set(_usdGeoTiff_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/usdGeoTiff")
install(EXPORT usdGeoTiffTargets FILE usdGeoTiffTargets.cmake
    NAMESPACE usdgeotiff:: DESTINATION "${_usdGeoTiff_cmakedir}")
configure_package_config_file(cmake/usdGeoTiffConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/usdGeoTiffConfig.cmake"
    INSTALL_DESTINATION "${_usdGeoTiff_cmakedir}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/usdGeoTiffConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/usdGeoTiffConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/usdGeoTiffConfigVersion.cmake"
    DESTINATION "${_usdGeoTiff_cmakedir}")

if(USDRASTER_BUILD_TESTS)
    add_subdirectory(tests)
endif()