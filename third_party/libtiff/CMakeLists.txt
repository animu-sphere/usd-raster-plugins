option(USDRASTER_USE_SYSTEM_LIBTIFF
       "Use a system-provided libtiff for the GeoTIFF decoder" ON)

if(NOT USDRASTER_USE_SYSTEM_LIBTIFF)
    message(FATAL_ERROR
        "A vendored libtiff source tree is not present; use the system libtiff adapter.")
endif()

find_package(TIFF 4.0 REQUIRED)
if(NOT TARGET TIFF::TIFF)
    message(FATAL_ERROR "FindTIFF did not provide the expected TIFF::TIFF target")
endif()

add_library(usd-raster-libtiff INTERFACE)
add_library(usd-raster::libtiff ALIAS usd-raster-libtiff)
target_link_libraries(usd-raster-libtiff INTERFACE TIFF::TIFF)