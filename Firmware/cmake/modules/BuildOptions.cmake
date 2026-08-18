function(wtk_configure_c_target target)
    set_target_properties(${target} PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )
endfunction()

function(wtk_apply_common_compile_options target)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -ffunction-sections
            -fdata-sections
        )
    endif()
endfunction()

function(wtk_detect_git_commit out_var)
    find_package(Git QUIET)
    if(Git_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE git_result
            OUTPUT_VARIABLE git_commit
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(git_result EQUAL 0 AND NOT git_commit STREQUAL "")
            set(${out_var} "${git_commit}" PARENT_SCOPE)
            return()
        endif()
    endif()

    set(${out_var} "unknown" PARENT_SCOPE)
endfunction()
