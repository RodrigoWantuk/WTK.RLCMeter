function(wtk_apply_project_warnings target)
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /W4)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wsign-conversion
            -Wdouble-promotion
            -Wformat=2
            -Wundef
        )
    endif()
endfunction()

function(wtk_treat_warnings_as_errors target)
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /WX)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()
