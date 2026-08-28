cmake_minimum_required(VERSION 3.25)

project(WTKRLCMeterFirmware
    VERSION 0.1.0
    DESCRIPTION "Firmware for the WTK.RLCMeter STM32F103C8T6 instrument"
    LANGUAGES C
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(BuildOptions)
include(CompilerWarnings)
include(STM32CubeF1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 COMPONENTS Interpreter QUIET)

set(WTK_TARGET_PLATFORM "host" CACHE STRING "Build target platform: host or stm32")
set_property(CACHE WTK_TARGET_PLATFORM PROPERTY STRINGS host stm32)
set(WTK_FIRMWARE_PROFILE "PRODUCT" CACHE STRING "Firmware feature profile: PRODUCT or BRINGUP")
set_property(CACHE WTK_FIRMWARE_PROFILE PROPERTY STRINGS PRODUCT BRINGUP)

option(WTK_WARNINGS_AS_ERRORS_HOST "Treat warnings as errors for host project targets" ON)
option(WTK_WARNINGS_AS_ERRORS_STM32 "Treat warnings as errors for STM32 project targets" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

wtk_detect_git_commit(WTK_GIT_COMMIT)

set(WTK_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
if(NOT WTK_BUILD_TYPE)
    set(WTK_BUILD_TYPE "MultiConfig")
endif()
string(TOUPPER "${WTK_FIRMWARE_PROFILE}" WTK_FIRMWARE_PROFILE)

if(WTK_BUILD_TYPE STREQUAL "Lab")
    message(FATAL_ERROR "CMAKE_BUILD_TYPE=Lab is obsolete. Use a standard build type plus WTK_FIRMWARE_PROFILE=BRINGUP.")
endif()

if(WTK_FIRMWARE_PROFILE STREQUAL "BRINGUP")
    set(WTK_DIAGNOSTIC_LOG_LEVEL_DEFAULT 5)
    set(WTK_ENABLE_BRINGUP_CONSOLE 1)
elseif(WTK_BUILD_TYPE STREQUAL "Release")
    set(WTK_DIAGNOSTIC_LOG_LEVEL_DEFAULT 2)
    set(WTK_ENABLE_BRINGUP_CONSOLE 0)
else()
    set(WTK_DIAGNOSTIC_LOG_LEVEL_DEFAULT 4)
    set(WTK_ENABLE_BRINGUP_CONSOLE 0)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/wtk_build_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/wtk_build_config.h"
    @ONLY
)

add_library(wtk_firmware_common STATIC
    src/app/app_safety_fault.c
    src/app/app_calibration_runtime.c
    src/app/app_calibration_service.c
    src/app/app_calibration_session.c
    src/app/app_calibration_campaign.c
    src/app/app_calibration_wizard.c
    src/app/app_calibration_workflow.c
    src/app/app_io_workspace.c
    src/app/app_measurement_session.c
    src/app/app_product.c
    src/app/app_settings_service.c
    src/app/app_version.c
    src/bsp/bsp_adc_common.c
    src/bsp/bsp_adc_core.c
    src/bsp/bsp_status.c
    src/drivers/buttons.c
    src/drivers/ili9341_color.c
    src/drivers/ili9341_geometry.c
    src/drivers/w25q_core.c
    src/hardware/hw_aux_sensors.c
    src/hardware/hw_buzzer_policy.c
    src/hardware/hw_charger.c
    src/hardware/hw_k1.c
    src/hardware/hw_k2.c
    src/hardware/hw_measure_permit.c
    src/hardware/hw_metrology_clock.c
    src/hardware/hw_excitation.c
    src/hardware/hw_metrology_raw.c
    src/hardware/hw_metrology_session.c
    src/hardware/hw_metrology_measure.c
    src/hardware/hw_power.c
    src/hardware/hw_range.c
    src/hardware/hw_residual.c
    src/hardware/hw_safety.c
    src/measurement/measurement_dsp.c
    src/measurement/measurement_condition.c
    src/measurement/measurement_calibration.c
    src/measurement/measurement_calibration_solver.c
    src/measurement/measurement_calibration_store.c
    src/measurement/measurement_engine.c
    src/storage/resource_store.c
    src/storage/storage_crc32.c
    src/storage/storage_layout.c
    src/ui/ui_fallback_font.c
    src/ui/ui_format.c
    src/ui/ui_product.c
    src/ui/ui_resources.c
    src/ui/ui_text.c
)
target_include_directories(wtk_firmware_common
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
wtk_configure_c_target(wtk_firmware_common)
wtk_apply_project_warnings(wtk_firmware_common)
wtk_apply_common_compile_options(wtk_firmware_common)

if(WTK_TARGET_PLATFORM STREQUAL "host")
    if(WTK_WARNINGS_AS_ERRORS_HOST)
        wtk_treat_warnings_as_errors(wtk_firmware_common)
    endif()

    enable_testing()
    add_subdirectory(tests)
elseif(WTK_TARGET_PLATFORM STREQUAL "stm32")
    if((NOT WTK_FIRMWARE_PROFILE STREQUAL "BRINGUP") AND
       (NOT WTK_FIRMWARE_PROFILE STREQUAL "PRODUCT"))
        message(FATAL_ERROR "Unsupported WTK_FIRMWARE_PROFILE='${WTK_FIRMWARE_PROFILE}'. Use PRODUCT or BRINGUP.")
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "GNU")
        target_compile_options(wtk_firmware_common PRIVATE -fstack-usage)
    endif()
    if(WTK_FIRMWARE_PROFILE STREQUAL "PRODUCT")
        # PRODUCT Debug intentionally uses -Os + debug symbols: the complete
        # firmware must fit the guaranteed 64 KiB STM32F103C8T6 Flash while
        # still preserving symbols for SWD/GDB inspection. Release may also
        # enable preset-level LTO for additional dead-code folding.
        target_compile_options(wtk_firmware_common PRIVATE -Os -fno-unwind-tables -fno-asynchronous-unwind-tables)
    endif()

    add_executable(wtk_rlcmeter
        src/app/app_shell.c
        src/app/main.c
        src/bsp/bsp_clock.c
        src/bsp/bsp_adc.c
        src/bsp/bsp_excitation.c
        src/bsp/bsp_metrology_adc.c
        src/bsp/bsp_diagnostics.c
        src/bsp/bsp_gpio.c
        src/bsp/bsp_quiet.c
        src/bsp/bsp_reset.c
        src/bsp/bsp_spi.c
        src/bsp/bsp_timers.c
        src/bsp/bsp_time.c
        src/bsp/bsp_uart.c
        src/bsp/bsp_watchdog.c
        src/drivers/ili9341.c
        src/drivers/spi_bus.c
        src/drivers/w25q_device.c
        src/storage/measurement_cal_w25q_adapter.c
        src/hardware/hw_backlight.c
        src/hardware/hw_buzzer.c
        src/hardware/hw_peripherals.c
        src/ui/ui_fallback_renderer.c
        src/bsp/startup_stm32f103c8tx.c
        src/bsp/system_stm32f1xx.c
    )
    if(WTK_FIRMWARE_PROFILE STREQUAL "BRINGUP")
        target_sources(wtk_rlcmeter PRIVATE src/app/app_bringup_console.c)
    else()
        target_sources(wtk_rlcmeter PRIVATE src/storage/app_settings_w25q_adapter.c)
    endif()
    set_target_properties(wtk_rlcmeter PROPERTIES
        OUTPUT_NAME "WTK.RLCMeter"
        SUFFIX ".elf"
    )
    target_link_libraries(wtk_rlcmeter PRIVATE wtk_firmware_common)
    target_include_directories(wtk_rlcmeter PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )
    wtk_configure_c_target(wtk_rlcmeter)
    wtk_apply_project_warnings(wtk_rlcmeter)
    wtk_apply_common_compile_options(wtk_rlcmeter)
    wtk_configure_stm32cube_f1(wtk_rlcmeter)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU")
        target_compile_options(wtk_rlcmeter PRIVATE -fstack-usage)
    endif()
    if(WTK_FIRMWARE_PROFILE STREQUAL "PRODUCT")
        target_compile_options(wtk_rlcmeter PRIVATE -Os -fno-unwind-tables -fno-asynchronous-unwind-tables)
    endif()

    if(WTK_WARNINGS_AS_ERRORS_STM32)
        wtk_treat_warnings_as_errors(wtk_rlcmeter)
    endif()

    set_source_files_properties(src/bsp/startup_stm32f103c8tx.c PROPERTIES
        COMPILE_OPTIONS "-Wno-pedantic"
    )

    set(WTK_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/stm32/STM32F103C8Tx_FLASH.ld")
    set(WTK_FIRMWARE_BASE "${CMAKE_CURRENT_BINARY_DIR}/WTK.RLCMeter")
    set(WTK_FIRMWARE_MAP "${WTK_FIRMWARE_BASE}.map")
    set(WTK_SIZE_BUDGET "none")
    if(WTK_FIRMWARE_PROFILE STREQUAL "BRINGUP")
        set(WTK_SIZE_BUDGET "bringup")
    elseif(WTK_FIRMWARE_PROFILE STREQUAL "PRODUCT" AND NOT WTK_BUILD_TYPE STREQUAL "Debug")
        set(WTK_SIZE_BUDGET "product")
    endif()

    target_link_options(wtk_rlcmeter PRIVATE
        "-T${WTK_LINKER_SCRIPT}"
        "-nostartfiles"
        "--specs=nano.specs"
        "--specs=nosys.specs"
        "-Wl,--gc-sections"
        "-Wl,--print-memory-usage"
        "-Wl,-Map=${WTK_FIRMWARE_MAP}"
    )

    add_custom_command(TARGET wtk_rlcmeter POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:wtk_rlcmeter> "${WTK_FIRMWARE_BASE}.bin"
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:wtk_rlcmeter> "${WTK_FIRMWARE_BASE}.hex"
        COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:wtk_rlcmeter>
        BYPRODUCTS
            "${WTK_FIRMWARE_BASE}.bin"
            "${WTK_FIRMWARE_BASE}.hex"
        COMMENT "Generating STM32 programming artifacts"
    )
    if((NOT WTK_SIZE_BUDGET STREQUAL "none") AND NOT Python3_Interpreter_FOUND)
        message(FATAL_ERROR "Python3 is required to enforce the '${WTK_SIZE_BUDGET}' STM32 firmware size budget.")
    endif()

    if(Python3_Interpreter_FOUND)
        add_custom_command(TARGET wtk_rlcmeter POST_BUILD
            COMMAND ${Python3_EXECUTABLE}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/firmware_size.py"
                    $<TARGET_FILE:wtk_rlcmeter>
                    --budget ${WTK_SIZE_BUDGET}
                    --size-tool ${CMAKE_SIZE}
                    --objdump-tool ${CMAKE_OBJDUMP}
                    --nm-tool ${CMAKE_NM}
                    --json-out "${WTK_FIRMWARE_BASE}.size.json"
            BYPRODUCTS "${WTK_FIRMWARE_BASE}.size.json"
            COMMENT "Checking STM32 firmware size budget"
        )
        add_custom_command(TARGET wtk_rlcmeter POST_BUILD
            COMMAND ${Python3_EXECUTABLE}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_profile_symbols.py"
                    $<TARGET_FILE:wtk_rlcmeter>
                    --profile ${WTK_FIRMWARE_PROFILE}
                    --nm-tool ${CMAKE_NM}
            COMMENT "Checking STM32 firmware profile composition"
        )
    else()
        message(WARNING "Python3 not found; optional STM32 size/profile checks will not run for unbudgeted builds.")
    endif()
else()
    message(FATAL_ERROR "Unsupported WTK_TARGET_PLATFORM='${WTK_TARGET_PLATFORM}'. Use 'host' or 'stm32'.")
endif()
