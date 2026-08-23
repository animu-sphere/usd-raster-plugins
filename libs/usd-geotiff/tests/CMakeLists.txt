add_executable(usdGeoTiff_metadata_tests test_geotiff_metadata.cpp)
target_link_libraries(usdGeoTiff_metadata_tests PRIVATE usdgeotiff::core)
target_compile_definitions(usdGeoTiff_metadata_tests PRIVATE
    FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/generated")
if(MSVC)
    target_compile_options(usdGeoTiff_metadata_tests PRIVATE /utf-8 /EHsc)
endif()
add_test(NAME usdGeoTiff_metadata COMMAND usdGeoTiff_metadata_tests)