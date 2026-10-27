# Keep original lifts read-only and preserve paths for unaffected unity units.
function(drakengard_prepare_vu0_registers source_variable source_directory output_directory)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(preparer "${REPO_ROOT}/scripts/prepare_vu0_registers.py")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${preparer}" "${REPO_ROOT}/work/extracted-001/SLES_523.22" ${${source_variable}})
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${preparer}"
        --input-complete --source "${source_directory}" "${output_directory}"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Verified macro-VU register preparation failed")
    endif()
    # This directory is already on the runner's include path. Updating only
    # this new helper keeps unrelated unity units and the existing PCH stable.
    configure_file("${REPO_ROOT}/native/runner/ee_vu0_sub.h"
        "${CMAKE_BINARY_DIR}/private_headers/ee_vu0_sub.h" COPYONLY)
    set(prepared_sources)
    foreach(source IN LISTS ${source_variable})
        get_filename_component(name "${source}" NAME)
        if(EXISTS "${output_directory}/${name}")
            list(APPEND prepared_sources "${output_directory}/${name}")
        else()
            list(APPEND prepared_sources "${source}")
        endif()
    endforeach()
    set(${source_variable} "${prepared_sources}" PARENT_SCOPE)
endfunction()
