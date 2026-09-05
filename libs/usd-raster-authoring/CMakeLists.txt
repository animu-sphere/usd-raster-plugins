cmake_minimum_required(VERSION 3.23)

project(usdRasterAuthoring
    VERSION 0.2.0
    DESCRIPTION "Shared OpenUSD raster metadata authoring"
    LANGUAGES CXX)

if(NOT pxr_FOUND)
    find_package(pxr REQUIRED CONFIG)
endif()

add_library(usdRasterAuthoring STATIC
    src/MetadataAuthoring.cpp
    src/MeshAuthoring.cpp)
add_library(usdraster::authoring ALIAS usdRasterAuthoring)
set_target_properties(usdRasterAuthoring PROPERTIES
    EXPORT_NAME authoring
    POSITION_INDEPENDENT_CODE ON)
target_compile_features(usdRasterAuthoring PUBLIC cxx_std_17)
target_include_directories(usdRasterAuthoring PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")
target_link_libraries(usdRasterAuthoring PUBLIC usdraster::core)

set(_usd_targets)
foreach(_component IN ITEMS gf sdf tf vt)
    if(TARGET ${_component})
        list(APPEND _usd_targets ${_component})
    elseif(TARGET pxr::${_component})
        list(APPEND _usd_targets pxr::${_component})
    endif()
endforeach()
if(_usd_targets)
    target_link_libraries(usdRasterAuthoring PUBLIC ${_usd_targets})
elseif(PXR_LIBRARIES)
    target_link_libraries(usdRasterAuthoring PUBLIC ${PXR_LIBRARIES})
else()
    message(FATAL_ERROR "Could not locate OpenUSD targets for usdRasterAuthoring")
endif()

if(MSVC)
    target_compile_options(usdRasterAuthoring PRIVATE /utf-8 /EHsc)
endif()

if(USDRASTER_BUILD_TESTS)
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS usdRasterAuthoring EXPORT usdRasterAuthoringTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
set(_usdRasterAuthoring_cmakedir
    "${CMAKE_INSTALL_LIBDIR}/cmake/usdRasterAuthoring")
install(EXPORT usdRasterAuthoringTargets
    FILE usdRasterAuthoringTargets.cmake
    NAMESPACE usdraster::
    DESTINATION "${_usdRasterAuthoring_cmakedir}")
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/usdRasterAuthoringConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/usdRasterAuthoringConfig.cmake"
    INSTALL_DESTINATION "${_usdRasterAuthoring_cmakedir}")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/usdRasterAuthoringConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/usdRasterAuthoringConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/usdRasterAuthoringConfigVersion.cmake"
    DESTINATION "${_usdRasterAuthoring_cmakedir}")