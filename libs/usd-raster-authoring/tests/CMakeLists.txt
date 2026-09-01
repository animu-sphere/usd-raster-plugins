add_executable(usdRasterAuthoring_mesh_tests test_mesh_authoring.cpp)
target_link_libraries(usdRasterAuthoring_mesh_tests PRIVATE usdraster::authoring)
if(MSVC)
    target_compile_options(usdRasterAuthoring_mesh_tests PRIVATE /utf-8 /EHsc)
endif()
add_test(NAME usdRasterAuthoring_mesh COMMAND usdRasterAuthoring_mesh_tests)

if(WIN32)
    set(_usd_runtime_directories)
    foreach(_usd_component IN ITEMS gf sdf tf vt)
        if(TARGET ${_usd_component})
            list(APPEND _usd_runtime_directories
                "$<TARGET_FILE_DIR:${_usd_component}>")
        elseif(TARGET pxr::${_usd_component})
            list(APPEND _usd_runtime_directories
                "$<TARGET_FILE_DIR:pxr::${_usd_component}>")
        endif()
    endforeach()
    foreach(_runtime_target IN ITEMS
            TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy)
        if(TARGET ${_runtime_target})
            list(APPEND _usd_runtime_directories
                "$<TARGET_FILE_DIR:${_runtime_target}>")
        endif()
    endforeach()
    foreach(_python_path IN ITEMS Python3_EXECUTABLE Python3_LIBRARY)
        if(${_python_path})
            get_filename_component(_python_runtime_directory
                "${${_python_path}}" DIRECTORY)
            list(APPEND _usd_runtime_directories
                "${_python_runtime_directory}")
        endif()
    endforeach()
    if(_usd_runtime_directories)
        set(_usd_path_modifications)
        foreach(_usd_runtime_directory IN LISTS _usd_runtime_directories)
            list(APPEND _usd_path_modifications
                "PATH=path_list_prepend:${_usd_runtime_directory}")
        endforeach()
        set_tests_properties(usdRasterAuthoring_mesh
            PROPERTIES ENVIRONMENT_MODIFICATION "${_usd_path_modifications}")
    endif()
endif()