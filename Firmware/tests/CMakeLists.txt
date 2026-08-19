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

add_executable(wtk_hw_safety_test
    hardware/test_hw_safety.c
)
target_link_libraries(wtk_hw_safety_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_hw_safety_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_hw_safety_test)
wtk_apply_project_warnings(wtk_hw_safety_test)
wtk_apply_common_compile_options(wtk_hw_safety_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_safety_test)
endif()

add_test(NAME wtk_hw_safety_test COMMAND wtk_hw_safety_test)

add_executable(wtk_hw_relays_test
    hardware/test_hw_relays.c
)
target_link_libraries(wtk_hw_relays_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_hw_relays_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_hw_relays_test)
wtk_apply_project_warnings(wtk_hw_relays_test)
wtk_apply_common_compile_options(wtk_hw_relays_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_relays_test)
endif()

add_test(NAME wtk_hw_relays_test COMMAND wtk_hw_relays_test)

add_executable(wtk_hw_range_test
    hardware/test_hw_range.c
)
target_link_libraries(wtk_hw_range_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_hw_range_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_hw_range_test)
wtk_apply_project_warnings(wtk_hw_range_test)
wtk_apply_common_compile_options(wtk_hw_range_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_range_test)
endif()

add_test(NAME wtk_hw_range_test COMMAND wtk_hw_range_test)

add_executable(wtk_hw_measure_permit_test
    hardware/test_hw_measure_permit.c
)
target_link_libraries(wtk_hw_measure_permit_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_hw_measure_permit_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_hw_measure_permit_test)
wtk_apply_project_warnings(wtk_hw_measure_permit_test)
wtk_apply_common_compile_options(wtk_hw_measure_permit_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_measure_permit_test)
endif()

add_test(NAME wtk_hw_measure_permit_test COMMAND wtk_hw_measure_permit_test)

add_executable(wtk_app_safety_fault_test
    app/test_app_safety_fault.c
)
target_link_libraries(wtk_app_safety_fault_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_app_safety_fault_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_app_safety_fault_test)
wtk_apply_project_warnings(wtk_app_safety_fault_test)
wtk_apply_common_compile_options(wtk_app_safety_fault_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_app_safety_fault_test)
endif()

add_test(NAME wtk_app_safety_fault_test COMMAND wtk_app_safety_fault_test)

add_executable(wtk_bsp_adc_core_test
    bsp/test_bsp_adc_core.c
)
target_link_libraries(wtk_bsp_adc_core_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_bsp_adc_core_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_bsp_adc_core_test)
wtk_apply_project_warnings(wtk_bsp_adc_core_test)
wtk_apply_common_compile_options(wtk_bsp_adc_core_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_bsp_adc_core_test)
endif()

add_test(NAME wtk_bsp_adc_core_test COMMAND wtk_bsp_adc_core_test)

add_executable(wtk_hw_aux_sensors_test
    hardware/test_hw_aux_sensors.c
)
target_link_libraries(wtk_hw_aux_sensors_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_hw_aux_sensors_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_hw_aux_sensors_test)
wtk_apply_project_warnings(wtk_hw_aux_sensors_test)
wtk_apply_common_compile_options(wtk_hw_aux_sensors_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_aux_sensors_test)
endif()

add_test(NAME wtk_hw_aux_sensors_test COMMAND wtk_hw_aux_sensors_test)
