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

add_executable(wtk_ili9341_color_test
    drivers/test_ili9341_color.c
)
target_link_libraries(wtk_ili9341_color_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_ili9341_color_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_ili9341_color_test)
wtk_apply_project_warnings(wtk_ili9341_color_test)
wtk_apply_common_compile_options(wtk_ili9341_color_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_ili9341_color_test)
endif()

add_test(NAME wtk_ili9341_color_test COMMAND wtk_ili9341_color_test)

add_executable(wtk_ili9341_geometry_test
    drivers/test_ili9341_geometry.c
)
target_link_libraries(wtk_ili9341_geometry_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_ili9341_geometry_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_ili9341_geometry_test)
wtk_apply_project_warnings(wtk_ili9341_geometry_test)
wtk_apply_common_compile_options(wtk_ili9341_geometry_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_ili9341_geometry_test)
endif()

add_test(NAME wtk_ili9341_geometry_test COMMAND wtk_ili9341_geometry_test)

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

add_executable(wtk_hw_buzzer_policy_test
    hardware/test_hw_buzzer_policy.c
)
target_link_libraries(wtk_hw_buzzer_policy_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_hw_buzzer_policy_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_hw_buzzer_policy_test)
wtk_apply_project_warnings(wtk_hw_buzzer_policy_test)
wtk_apply_common_compile_options(wtk_hw_buzzer_policy_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_buzzer_policy_test)
endif()

add_test(NAME wtk_hw_buzzer_policy_test COMMAND wtk_hw_buzzer_policy_test)
