# Local fixes to the upstream submodules, kept as patch files in
# lib-patches/<name>/ and applied to the submodule's working tree at
# configure time (each only once: skipped when it already reverse-applies).
# The submodule is lib/<name> unless a folder (relative to the repo root) is
# given, as for submodules nested in another one.
find_package(Git REQUIRED)

function(btx_apply_lib_patches submodule)
    if (ARGC GREATER 1)
        set(dir "${CMAKE_SOURCE_DIR}/${ARGV1}")
    else()
        set(dir "${CMAKE_SOURCE_DIR}/lib/${submodule}")
    endif()
    file(GLOB patches "${CMAKE_SOURCE_DIR}/lib-patches/${submodule}/*.patch")
    list(SORT patches)
    foreach(patch IN LISTS patches)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace --reverse --check "${patch}"
            WORKING_DIRECTORY "${dir}"
            RESULT_VARIABLE already_applied OUTPUT_QUIET ERROR_QUIET)
        if (already_applied EQUAL 0)
            continue()
        endif()
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace "${patch}"
            WORKING_DIRECTORY "${dir}"
            RESULT_VARIABLE result ERROR_VARIABLE error)
        if (NOT result EQUAL 0)
            message(FATAL_ERROR "Couldn't apply ${patch} to ${dir}:\n${error}")
        endif()
        message(STATUS "Applied ${patch} to ${dir}")
    endforeach()
endfunction()
