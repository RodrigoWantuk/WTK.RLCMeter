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

add_executable(wtk_buttons_test
    drivers/test_buttons.c
)
target_link_libraries(wtk_buttons_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_buttons_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_buttons_test)
wtk_apply_project_warnings(wtk_buttons_test)
wtk_apply_common_compile_options(wtk_buttons_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_buttons_test)
endif()

add_test(NAME wtk_buttons_test COMMAND wtk_buttons_test)

add_executable(wtk_w25q_core_test
    drivers/test_w25q_core.c
)
target_link_libraries(wtk_w25q_core_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_w25q_core_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_w25q_core_test)
wtk_apply_project_warnings(wtk_w25q_core_test)
wtk_apply_common_compile_options(wtk_w25q_core_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_w25q_core_test)
endif()

add_test(NAME wtk_w25q_core_test COMMAND wtk_w25q_core_test)

add_executable(wtk_resource_store_test
    storage/test_resource_store.c
)
target_link_libraries(wtk_resource_store_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_resource_store_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_resource_store_test)
wtk_apply_project_warnings(wtk_resource_store_test)
wtk_apply_common_compile_options(wtk_resource_store_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_resource_store_test)
endif()

add_test(NAME wtk_resource_store_test COMMAND wtk_resource_store_test)
