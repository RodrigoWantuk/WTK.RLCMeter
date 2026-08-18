add_executable(wtk_smoke_test
    smoke/test_version.c
)
target_link_libraries(wtk_smoke_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_smoke_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_smoke_test)
wtk_apply_project_warnings(wtk_smoke_test)
wtk_apply_common_compile_options(wtk_smoke_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_smoke_test)
endif()

add_test(NAME wtk_smoke_test COMMAND wtk_smoke_test)
