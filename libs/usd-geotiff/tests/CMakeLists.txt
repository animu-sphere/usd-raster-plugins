find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    set(_usdGeoTiff_fixture_dir "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
    add_custom_command(
        OUTPUT "${_usdGeoTiff_fixture_dir}/MANIFEST.sha256"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_usdGeoTiff_fixture_dir}"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/generate_fixtures.py"
                --out "${_usdGeoTiff_fixture_dir}"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/generate_fixtures.py"
        VERBATIM)
    add_custom_target(usdGeoTiff_test_fixtures
        DEPENDS "${_usdGeoTiff_fixture_dir}/MANIFEST.sha256")

    add_executable(usdGeoTiff_metadata_tests test_geotiff_metadata.cpp)
    add_dependencies(usdGeoTiff_metadata_tests usdGeoTiff_test_fixtures)
    target_link_libraries(usdGeoTiff_metadata_tests PRIVATE usdgeotiff::core)
    target_compile_definitions(usdGeoTiff_metadata_tests PRIVATE
        FIXTURE_DIR="${_usdGeoTiff_fixture_dir}")
    if(TARGET usd-raster::libtiff)
        target_compile_definitions(usdGeoTiff_metadata_tests PRIVATE
            USDRASTER_HAS_LIBTIFF=1)
    endif()
    if(MSVC)
        target_compile_options(usdGeoTiff_metadata_tests PRIVATE /utf-8 /EHsc)
    endif()
    add_test(NAME usdGeoTiff_metadata COMMAND usdGeoTiff_metadata_tests)

    add_executable(usdGeoTiff_pixel_tests test_geotiff_pixel.cpp)
    add_dependencies(usdGeoTiff_pixel_tests usdGeoTiff_test_fixtures)
    target_link_libraries(usdGeoTiff_pixel_tests PRIVATE usdgeotiff::core)
    target_compile_definitions(usdGeoTiff_pixel_tests PRIVATE
        FIXTURE_DIR="${_usdGeoTiff_fixture_dir}")
    if(TARGET usd-raster::libtiff)
        target_compile_definitions(usdGeoTiff_pixel_tests PRIVATE
            USDRASTER_HAS_LIBTIFF=1)
    endif()
    if(MSVC)
        target_compile_options(usdGeoTiff_pixel_tests PRIVATE /utf-8 /EHsc)
    endif()
    add_test(NAME usdGeoTiff_pixel COMMAND usdGeoTiff_pixel_tests)
else()
    message(STATUS "Python3 not found; skipping usdGeoTiff metadata tests")
endif()