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

add_executable(wtk_measurement_trimmed_test measurement/test_measurement_trimmed.c)
target_link_libraries(wtk_measurement_trimmed_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_measurement_trimmed_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
wtk_configure_c_target(wtk_measurement_trimmed_test)
wtk_apply_project_warnings(wtk_measurement_trimmed_test)
wtk_apply_common_compile_options(wtk_measurement_trimmed_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_measurement_trimmed_test)
endif()
add_test(NAME wtk_measurement_trimmed_test COMMAND wtk_measurement_trimmed_test)

if(WTK_PRODUCT_MODERN_VALUE)
    add_executable(wtk_measurement_survey_test measurement/test_measurement_survey.c)
    target_link_libraries(wtk_measurement_survey_test PRIVATE wtk_firmware_common)
    target_include_directories(wtk_measurement_survey_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    wtk_configure_c_target(wtk_measurement_survey_test)
    wtk_apply_project_warnings(wtk_measurement_survey_test)
    wtk_apply_common_compile_options(wtk_measurement_survey_test)
    if(WTK_WARNINGS_AS_ERRORS_HOST)
        wtk_treat_warnings_as_errors(wtk_measurement_survey_test)
    endif()
    add_test(NAME wtk_measurement_survey_test COMMAND wtk_measurement_survey_test)
endif()

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

add_executable(wtk_ui_peripheral_test ui/test_ui_peripheral_test.c)
target_link_libraries(wtk_ui_peripheral_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_ui_peripheral_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
wtk_configure_c_target(wtk_ui_peripheral_test)
wtk_apply_project_warnings(wtk_ui_peripheral_test)
wtk_apply_common_compile_options(wtk_ui_peripheral_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_ui_peripheral_test)
endif()
add_test(NAME wtk_ui_peripheral_test COMMAND wtk_ui_peripheral_test)

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

add_executable(wtk_ili9341_readback_test drivers/test_ili9341_readback.c ../src/drivers/ili9341.c)
target_link_libraries(wtk_ili9341_readback_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_ili9341_readback_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
wtk_configure_c_target(wtk_ili9341_readback_test)
wtk_apply_project_warnings(wtk_ili9341_readback_test)
wtk_apply_common_compile_options(wtk_ili9341_readback_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_ili9341_readback_test)
endif()
add_test(NAME wtk_ili9341_readback_test COMMAND wtk_ili9341_readback_test)

add_executable(wtk_spi_bus_clock_test drivers/test_spi_bus_clock.c ../src/drivers/spi_bus.c)
target_include_directories(wtk_spi_bus_clock_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
wtk_configure_c_target(wtk_spi_bus_clock_test)
wtk_apply_project_warnings(wtk_spi_bus_clock_test)
wtk_apply_common_compile_options(wtk_spi_bus_clock_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_spi_bus_clock_test)
endif()
add_test(NAME wtk_spi_bus_clock_test COMMAND wtk_spi_bus_clock_test)

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

add_executable(wtk_hw_k1_bench_test hardware/test_hw_k1_bench.c)
target_link_libraries(wtk_hw_k1_bench_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_hw_k1_bench_test PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_BINARY_DIR}/generated")
wtk_configure_c_target(wtk_hw_k1_bench_test)
wtk_apply_project_warnings(wtk_hw_k1_bench_test)
wtk_apply_common_compile_options(wtk_hw_k1_bench_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_k1_bench_test)
endif()
add_test(NAME wtk_hw_k1_bench_test COMMAND wtk_hw_k1_bench_test)

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

add_executable(wtk_app_flash_access_test
    app/test_app_flash_access.c
)
target_link_libraries(wtk_app_flash_access_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_app_flash_access_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_app_flash_access_test)
wtk_apply_project_warnings(wtk_app_flash_access_test)
wtk_apply_common_compile_options(wtk_app_flash_access_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_app_flash_access_test)
endif()

add_test(NAME wtk_app_flash_access_test COMMAND wtk_app_flash_access_test)

add_executable(wtk_app_io_workspace_test
    app/test_app_io_workspace.c
)
target_link_libraries(wtk_app_io_workspace_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_app_io_workspace_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_app_io_workspace_test)
wtk_apply_project_warnings(wtk_app_io_workspace_test)
wtk_apply_common_compile_options(wtk_app_io_workspace_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_app_io_workspace_test)
endif()

add_test(NAME wtk_app_io_workspace_test COMMAND wtk_app_io_workspace_test)

add_executable(wtk_app_pc_link_protocol_test
    app/test_app_pc_link_protocol.c
)
target_link_libraries(wtk_app_pc_link_protocol_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_app_pc_link_protocol_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_app_pc_link_protocol_test)
wtk_apply_project_warnings(wtk_app_pc_link_protocol_test)
wtk_apply_common_compile_options(wtk_app_pc_link_protocol_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_app_pc_link_protocol_test)
endif()

add_test(NAME wtk_app_pc_link_protocol_test COMMAND wtk_app_pc_link_protocol_test)

add_executable(wtk_app_resource_update_test
    app/test_app_resource_update.c
)
target_link_libraries(wtk_app_resource_update_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_app_resource_update_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_app_resource_update_test)
wtk_apply_project_warnings(wtk_app_resource_update_test)
wtk_apply_common_compile_options(wtk_app_resource_update_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_app_resource_update_test)
endif()

add_test(NAME wtk_app_resource_update_test COMMAND wtk_app_resource_update_test)

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

add_executable(wtk_hw_metrology_clock_test
    hardware/test_hw_metrology_clock.c
)
target_link_libraries(wtk_hw_metrology_clock_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_hw_metrology_clock_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_hw_metrology_clock_test)
wtk_apply_project_warnings(wtk_hw_metrology_clock_test)
wtk_apply_common_compile_options(wtk_hw_metrology_clock_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_metrology_clock_test)
endif()

add_test(NAME wtk_hw_metrology_clock_test COMMAND wtk_hw_metrology_clock_test)

add_executable(wtk_hw_excitation_test
    hardware/test_hw_excitation.c
)
target_link_libraries(wtk_hw_excitation_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_hw_excitation_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_hw_excitation_test)
wtk_apply_project_warnings(wtk_hw_excitation_test)
wtk_apply_common_compile_options(wtk_hw_excitation_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_excitation_test)
endif()

add_test(NAME wtk_hw_excitation_test COMMAND wtk_hw_excitation_test)

add_executable(wtk_hw_metrology_raw_test
    hardware/test_hw_metrology_raw.c
)
target_link_libraries(wtk_hw_metrology_raw_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_hw_metrology_raw_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_hw_metrology_raw_test)
wtk_apply_project_warnings(wtk_hw_metrology_raw_test)
wtk_apply_common_compile_options(wtk_hw_metrology_raw_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_metrology_raw_test)
endif()

add_test(NAME wtk_hw_metrology_raw_test COMMAND wtk_hw_metrology_raw_test)

add_executable(wtk_hw_metrology_session_test
    hardware/test_hw_metrology_session.c
)
target_link_libraries(wtk_hw_metrology_session_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_hw_metrology_session_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_hw_metrology_session_test)
wtk_apply_project_warnings(wtk_hw_metrology_session_test)
wtk_apply_common_compile_options(wtk_hw_metrology_session_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_metrology_session_test)
endif()

add_test(NAME wtk_hw_metrology_session_test COMMAND wtk_hw_metrology_session_test)

add_executable(wtk_hw_metrology_measure_test
    hardware/test_hw_metrology_measure.c
)
target_link_libraries(wtk_hw_metrology_measure_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_hw_metrology_measure_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_hw_metrology_measure_test)
wtk_apply_project_warnings(wtk_hw_metrology_measure_test)
wtk_apply_common_compile_options(wtk_hw_metrology_measure_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_metrology_measure_test)
endif()

add_test(NAME wtk_hw_metrology_measure_test COMMAND wtk_hw_metrology_measure_test)

add_executable(wtk_measurement_dsp_test
    measurement/test_measurement_dsp.c
)
target_link_libraries(wtk_measurement_dsp_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_measurement_dsp_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_measurement_dsp_test)
wtk_apply_project_warnings(wtk_measurement_dsp_test)
wtk_apply_common_compile_options(wtk_measurement_dsp_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_measurement_dsp_test)
endif()

add_test(NAME wtk_measurement_dsp_test COMMAND wtk_measurement_dsp_test)

if(WTK_ENABLE_SUPPLEMENTARY_CURVES)
add_executable(wtk_measurement_cal_curve_test
    measurement/test_measurement_cal_curve.c
)
target_link_libraries(wtk_measurement_cal_curve_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_measurement_cal_curve_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
wtk_configure_c_target(wtk_measurement_cal_curve_test)
wtk_apply_project_warnings(wtk_measurement_cal_curve_test)
wtk_apply_common_compile_options(wtk_measurement_cal_curve_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_measurement_cal_curve_test)
endif()
add_test(NAME wtk_measurement_cal_curve_test COMMAND wtk_measurement_cal_curve_test)

add_executable(wtk_measurement_cal_curve_frame_test
    measurement/test_measurement_cal_curve_frame.c
)
target_link_libraries(wtk_measurement_cal_curve_frame_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_measurement_cal_curve_frame_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
wtk_configure_c_target(wtk_measurement_cal_curve_frame_test)
wtk_apply_project_warnings(wtk_measurement_cal_curve_frame_test)
wtk_apply_common_compile_options(wtk_measurement_cal_curve_frame_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_measurement_cal_curve_frame_test)
endif()
add_test(NAME wtk_measurement_cal_curve_frame_test COMMAND wtk_measurement_cal_curve_frame_test)

add_executable(wtk_measurement_cal_curve_store_test
    measurement/test_measurement_cal_curve_store.c
)
target_link_libraries(wtk_measurement_cal_curve_store_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_measurement_cal_curve_store_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
wtk_configure_c_target(wtk_measurement_cal_curve_store_test)
wtk_apply_project_warnings(wtk_measurement_cal_curve_store_test)
wtk_apply_common_compile_options(wtk_measurement_cal_curve_store_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_measurement_cal_curve_store_test)
endif()
add_test(NAME wtk_measurement_cal_curve_store_test COMMAND wtk_measurement_cal_curve_store_test)
endif()

add_executable(wtk_measurement_dc_test
    measurement/test_measurement_dc.c
)
target_link_libraries(wtk_measurement_dc_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_measurement_dc_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
wtk_configure_c_target(wtk_measurement_dc_test)
wtk_apply_project_warnings(wtk_measurement_dc_test)
wtk_apply_common_compile_options(wtk_measurement_dc_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_measurement_dc_test)
endif()
add_test(NAME wtk_measurement_dc_test COMMAND wtk_measurement_dc_test)

add_executable(wtk_measurement_calibration_test
    measurement/test_measurement_calibration.c
)
target_link_libraries(wtk_measurement_calibration_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_measurement_calibration_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_measurement_calibration_test)
wtk_apply_project_warnings(wtk_measurement_calibration_test)
wtk_apply_common_compile_options(wtk_measurement_calibration_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_measurement_calibration_test)
endif()

add_test(NAME wtk_measurement_calibration_test COMMAND wtk_measurement_calibration_test)

add_executable(wtk_measurement_engine_test
    measurement/test_measurement_engine.c
)
target_link_libraries(wtk_measurement_engine_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_measurement_engine_test PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_measurement_engine_test)
wtk_apply_project_warnings(wtk_measurement_engine_test)
wtk_apply_common_compile_options(wtk_measurement_engine_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_measurement_engine_test)
endif()

add_test(NAME wtk_measurement_engine_test COMMAND wtk_measurement_engine_test)

add_executable(wtk_app_measurement_session_test
    app/test_app_measurement_session.c
)
target_link_libraries(wtk_app_measurement_session_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_app_measurement_session_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
wtk_configure_c_target(wtk_app_measurement_session_test)
wtk_apply_project_warnings(wtk_app_measurement_session_test)
wtk_apply_common_compile_options(wtk_app_measurement_session_test)

if(WTK_WARNINGS_AS_ERRORS)
    wtk_treat_warnings_as_errors(wtk_app_measurement_session_test)
endif()

add_test(NAME wtk_app_measurement_session_test COMMAND wtk_app_measurement_session_test)

add_executable(wtk_app_product_test
    app/test_app_product.c
)
target_link_libraries(wtk_app_product_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_app_product_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
wtk_configure_c_target(wtk_app_product_test)
wtk_apply_project_warnings(wtk_app_product_test)
wtk_apply_common_compile_options(wtk_app_product_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_app_product_test)
endif()

add_test(NAME wtk_app_product_test COMMAND wtk_app_product_test)

add_executable(wtk_app_settings_service_test
    app/test_app_settings_service.c
)
target_link_libraries(wtk_app_settings_service_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_app_settings_service_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/src"
)
wtk_configure_c_target(wtk_app_settings_service_test)
wtk_apply_project_warnings(wtk_app_settings_service_test)
wtk_apply_common_compile_options(wtk_app_settings_service_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_app_settings_service_test)
endif()
add_test(NAME wtk_app_settings_service_test COMMAND wtk_app_settings_service_test)

add_executable(wtk_settings_w25q_contract_test storage/test_settings_w25q_contract.c
    ../src/drivers/w25q_device.c ../src/storage/app_settings_w25q_adapter.c
    ../src/storage/measurement_cal_w25q_adapter.c)
target_link_libraries(wtk_settings_w25q_contract_test PRIVATE wtk_firmware_common)
wtk_configure_c_target(wtk_settings_w25q_contract_test)
wtk_apply_project_warnings(wtk_settings_w25q_contract_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_settings_w25q_contract_test)
endif()
add_test(NAME wtk_settings_w25q_contract_test COMMAND wtk_settings_w25q_contract_test)

add_executable(wtk_factory_calibration_loadonly_test app/test_factory_calibration_loadonly.c
    ../src/app/app_calibration_service_loadonly.c ../src/measurement/measurement_calibration_store_loadonly.c)
target_compile_definitions(wtk_factory_calibration_loadonly_test PRIVATE WTK_CAL_FACTORY_LOAD_ONLY=1)
target_link_libraries(wtk_factory_calibration_loadonly_test PRIVATE wtk_firmware_common)
wtk_configure_c_target(wtk_factory_calibration_loadonly_test)
wtk_apply_project_warnings(wtk_factory_calibration_loadonly_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_factory_calibration_loadonly_test)
endif()
add_test(NAME wtk_factory_calibration_loadonly_test COMMAND wtk_factory_calibration_loadonly_test)

add_executable(wtk_app_feedback_test app/test_app_feedback.c)
target_link_libraries(wtk_app_feedback_test PRIVATE wtk_firmware_common)
wtk_configure_c_target(wtk_app_feedback_test)
wtk_apply_project_warnings(wtk_app_feedback_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_app_feedback_test)
endif()
add_test(NAME wtk_app_feedback_test COMMAND wtk_app_feedback_test)
add_executable(wtk_bsp_uart_rx_test bsp/test_bsp_uart_rx.c)
target_link_libraries(wtk_bsp_uart_rx_test PRIVATE wtk_firmware_common)
wtk_configure_c_target(wtk_bsp_uart_rx_test)
wtk_apply_project_warnings(wtk_bsp_uart_rx_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_bsp_uart_rx_test)
endif()
add_test(NAME wtk_bsp_uart_rx_test COMMAND wtk_bsp_uart_rx_test)
add_executable(wtk_serial_telemetry_test app/test_serial_telemetry.c)
target_link_libraries(wtk_serial_telemetry_test PRIVATE wtk_firmware_common)
wtk_configure_c_target(wtk_serial_telemetry_test)
wtk_apply_project_warnings(wtk_serial_telemetry_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_serial_telemetry_test)
endif()
add_test(NAME wtk_serial_telemetry_test COMMAND wtk_serial_telemetry_test)
add_executable(wtk_lead_compensation_test measurement/test_lead_compensation.c)
target_link_libraries(wtk_lead_compensation_test PRIVATE wtk_firmware_common)
if(NOT MSVC)
    target_link_libraries(wtk_lead_compensation_test PRIVATE m)
endif()
wtk_configure_c_target(wtk_lead_compensation_test)
wtk_apply_project_warnings(wtk_lead_compensation_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_lead_compensation_test)
endif()
add_test(NAME wtk_lead_compensation_test COMMAND wtk_lead_compensation_test)
foreach(audit_regression wtk_settings_w25q_contract_test wtk_factory_calibration_loadonly_test wtk_app_feedback_test wtk_bsp_uart_rx_test wtk_serial_telemetry_test wtk_lead_compensation_test)
    if(MSVC)
        target_compile_options(${audit_regression} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${audit_regression} PRIVATE -UNDEBUG)
    endif()
endforeach()

add_executable(wtk_app_calibration_wizard_test
    app/test_app_calibration_wizard.c
)
target_link_libraries(wtk_app_calibration_wizard_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_app_calibration_wizard_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
wtk_configure_c_target(wtk_app_calibration_wizard_test)
wtk_apply_project_warnings(wtk_app_calibration_wizard_test)
wtk_apply_common_compile_options(wtk_app_calibration_wizard_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_app_calibration_wizard_test)
endif()

add_test(NAME wtk_app_calibration_wizard_test COMMAND wtk_app_calibration_wizard_test)

add_executable(wtk_ui_format_test
    ui/test_ui_format.c
)
target_link_libraries(wtk_ui_format_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_ui_format_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
wtk_configure_c_target(wtk_ui_format_test)
wtk_apply_project_warnings(wtk_ui_format_test)
wtk_apply_common_compile_options(wtk_ui_format_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_ui_format_test)
endif()

add_test(NAME wtk_ui_format_test COMMAND wtk_ui_format_test)

add_executable(wtk_ui_utf8_test
    ui/test_ui_utf8.c
)
target_link_libraries(wtk_ui_utf8_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_ui_utf8_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
wtk_configure_c_target(wtk_ui_utf8_test)
wtk_apply_project_warnings(wtk_ui_utf8_test)
wtk_apply_common_compile_options(wtk_ui_utf8_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_ui_utf8_test)
endif()

add_test(NAME wtk_ui_utf8_test COMMAND wtk_ui_utf8_test)

add_executable(wtk_ui_font_test
    ui/test_ui_font.c
)
target_link_libraries(wtk_ui_font_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_ui_font_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
wtk_configure_c_target(wtk_ui_font_test)
wtk_apply_project_warnings(wtk_ui_font_test)
wtk_apply_common_compile_options(wtk_ui_font_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_ui_font_test)
endif()

add_test(NAME wtk_ui_font_test COMMAND wtk_ui_font_test)

add_executable(wtk_ui_image_test
    ui/test_ui_image.c
)
target_link_libraries(wtk_ui_image_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_ui_image_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
wtk_configure_c_target(wtk_ui_image_test)
wtk_apply_project_warnings(wtk_ui_image_test)
wtk_apply_common_compile_options(wtk_ui_image_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_ui_image_test)
endif()

add_test(NAME wtk_ui_image_test COMMAND wtk_ui_image_test)

add_executable(wtk_ui_product_test
    ui/test_ui_product.c
)
target_link_libraries(wtk_ui_product_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_ui_product_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
wtk_configure_c_target(wtk_ui_product_test)
wtk_apply_project_warnings(wtk_ui_product_test)
wtk_apply_common_compile_options(wtk_ui_product_test)

if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_ui_product_test)
endif()

add_test(NAME wtk_ui_product_test COMMAND wtk_ui_product_test)

add_executable(wtk_ui_product_preview
    ui/preview_ui_product.c
    ../src/ui/ui_fallback_renderer.c
)
target_link_libraries(wtk_ui_product_preview PRIVATE wtk_firmware_common)
target_include_directories(wtk_ui_product_preview PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
wtk_configure_c_target(wtk_ui_product_preview)
wtk_apply_project_warnings(wtk_ui_product_preview)
wtk_apply_common_compile_options(wtk_ui_product_preview)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_ui_product_preview)
endif()

add_executable(wtk_app_calibration_workflow_test
    app/test_app_calibration_workflow.c
)
target_link_libraries(wtk_app_calibration_workflow_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_app_calibration_workflow_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
wtk_configure_c_target(wtk_app_calibration_workflow_test)
wtk_apply_project_warnings(wtk_app_calibration_workflow_test)
wtk_apply_common_compile_options(wtk_app_calibration_workflow_test)

if(WTK_WARNINGS_AS_ERRORS)
    wtk_treat_warnings_as_errors(wtk_app_calibration_workflow_test)
endif()

add_test(NAME wtk_app_calibration_workflow_test COMMAND wtk_app_calibration_workflow_test)

# Host-only oracle: exercise the production solver, frame codec and A/B store.
add_executable(wtk_pc_osl_bridge measurement/pc_osl_bridge.c)
target_link_libraries(wtk_pc_osl_bridge PRIVATE wtk_firmware_common)
target_include_directories(wtk_pc_osl_bridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
wtk_configure_c_target(wtk_pc_osl_bridge)
wtk_apply_project_warnings(wtk_pc_osl_bridge)
wtk_apply_common_compile_options(wtk_pc_osl_bridge)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_pc_osl_bridge)
endif()
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
add_test(NAME wtk_pc_serial_ui_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_pc_serial_ui.py)
add_test(NAME wtk_ntc_factory_profile_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_ntc_factory_profile.py)
set(WTK_NTC_TEST_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/ntc-factory-generated")
file(MAKE_DIRECTORY "${WTK_NTC_TEST_GENERATED}")
execute_process(COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/ntc_factory_profile.py
    --profile ${PROJECT_SOURCE_DIR}/config/units/54ff70068370525760380567-ntc-engineering.json
    --header ${WTK_NTC_TEST_GENERATED}/wtk_ntc_factory_lut.h
    --metadata ${WTK_NTC_TEST_GENERATED}/profile.json
    RESULT_VARIABLE WTK_NTC_TEST_RESULT)
if(NOT WTK_NTC_TEST_RESULT EQUAL 0)
    message(FATAL_ERROR "NTC factory test data generation failed")
endif()
file(READ "${PROJECT_BINARY_DIR}/generated/wtk_build_config.h" WTK_NTC_TEST_CONFIG)
string(REPLACE "#define WTK_NTC_FACTORY_CALIBRATION 0u" "#define WTK_NTC_FACTORY_CALIBRATION 1u"
       WTK_NTC_TEST_CONFIG "${WTK_NTC_TEST_CONFIG}")
file(WRITE "${WTK_NTC_TEST_GENERATED}/wtk_build_config.h" "${WTK_NTC_TEST_CONFIG}")
add_executable(wtk_hw_ntc_factory_test hardware/test_hw_ntc_factory.c ../src/hardware/hw_power.c)
target_include_directories(wtk_hw_ntc_factory_test PRIVATE ${WTK_NTC_TEST_GENERATED} ${PROJECT_SOURCE_DIR}/src)
wtk_configure_c_target(wtk_hw_ntc_factory_test)
wtk_apply_project_warnings(wtk_hw_ntc_factory_test)
wtk_apply_common_compile_options(wtk_hw_ntc_factory_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_hw_ntc_factory_test)
endif()
add_test(NAME wtk_hw_ntc_factory_test COMMAND wtk_hw_ntc_factory_test)
if(WTK_PRODUCT_MODERN_VALUE)
    add_test(NAME wtk_product_ac_ui_integration
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/verify_product_ac_ui.py
                --controller $<TARGET_FILE:wtk_app_product_test>
                --preview $<TARGET_FILE:wtk_ui_product_preview>)
endif()
add_test(NAME wtk_pc_osl_compatibility
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/pc_osl_compatibility.py
            --bridge $<TARGET_FILE:wtk_pc_osl_bridge>)

add_executable(wtk_pc_cal_capture_bridge app/pc_cal_capture_bridge.c)
target_link_libraries(wtk_pc_cal_capture_bridge PRIVATE wtk_firmware_common)
target_include_directories(wtk_pc_cal_capture_bridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
wtk_configure_c_target(wtk_pc_cal_capture_bridge)
wtk_apply_project_warnings(wtk_pc_cal_capture_bridge)
wtk_apply_common_compile_options(wtk_pc_cal_capture_bridge)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_pc_cal_capture_bridge)
endif()
add_test(NAME wtk_pc_cal_capture_integration
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_pc_cal_capture.py
            --bridge $<TARGET_FILE:wtk_pc_cal_capture_bridge>)
add_test(NAME wtk_pc_osl_install_integration
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_pc_osl_install.py
            --bridge $<TARGET_FILE:wtk_pc_cal_capture_bridge>)

add_executable(wtk_lead_profile_store_test storage/test_lead_profile_store.c)
target_link_libraries(wtk_lead_profile_store_test PRIVATE wtk_firmware_common)
target_include_directories(wtk_lead_profile_store_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
wtk_configure_c_target(wtk_lead_profile_store_test)
wtk_apply_project_warnings(wtk_lead_profile_store_test)
wtk_apply_common_compile_options(wtk_lead_profile_store_test)
if(WTK_WARNINGS_AS_ERRORS_HOST)
    wtk_treat_warnings_as_errors(wtk_lead_profile_store_test)
endif()
if(MSVC)
    target_compile_options(wtk_lead_profile_store_test PRIVATE /UNDEBUG)
else()
    target_compile_options(wtk_lead_profile_store_test PRIVATE -UNDEBUG)
endif()
add_test(NAME wtk_lead_profile_store_test COMMAND wtk_lead_profile_store_test)

if(WTK_PRODUCT_MODERN_VALUE)
    add_executable(wtk_app_lead_calibration_test app/test_app_lead_calibration.c)
    target_link_libraries(wtk_app_lead_calibration_test PRIVATE wtk_firmware_common)
    if(NOT MSVC)
        target_link_libraries(wtk_app_lead_calibration_test PRIVATE m)
    endif()
    target_include_directories(wtk_app_lead_calibration_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
    wtk_configure_c_target(wtk_app_lead_calibration_test)
    wtk_apply_project_warnings(wtk_app_lead_calibration_test)
    wtk_apply_common_compile_options(wtk_app_lead_calibration_test)
    if(WTK_WARNINGS_AS_ERRORS_HOST)
        wtk_treat_warnings_as_errors(wtk_app_lead_calibration_test)
    endif()
    if(MSVC)
        target_compile_options(wtk_app_lead_calibration_test PRIVATE /UNDEBUG)
    else()
        target_compile_options(wtk_app_lead_calibration_test PRIVATE -UNDEBUG)
    endif()
    add_test(NAME wtk_app_lead_calibration_test COMMAND wtk_app_lead_calibration_test)
endif()

add_test(NAME wtk_pc_flash_snapshot_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_pc_flash_snapshot.py)
