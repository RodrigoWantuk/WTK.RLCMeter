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

set(WTK_TARGET_PLATFORM "host" CACHE STRING "Build target platform: host or stm32")
set_property(CACHE WTK_TARGET_PLATFORM PROPERTY STRINGS host stm32)

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

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/wtk_build_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/wtk_build_config.h"
    @ONLY
)

add_library(wtk_firmware_common STATIC
    src/app/app_version.c
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
    add_executable(wtk_rlcmeter
        src/app/main.c
        src/bsp/stm32_dependency_probe.c
        src/bsp/startup_stm32f103c8tx.c
        src/bsp/system_stm32f1xx.c
    )
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

    if(WTK_WARNINGS_AS_ERRORS_STM32)
        wtk_treat_warnings_as_errors(wtk_rlcmeter)
    endif()

    set_source_files_properties(src/bsp/startup_stm32f103c8tx.c PROPERTIES
        COMPILE_OPTIONS "-Wno-pedantic"
    )

    set(WTK_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/stm32/STM32F103C8Tx_FLASH.ld")
    set(WTK_FIRMWARE_BASE "${CMAKE_CURRENT_BINARY_DIR}/WTK.RLCMeter")
    set(WTK_FIRMWARE_MAP "${WTK_FIRMWARE_BASE}.map")

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
else()
    message(FATAL_ERROR "Unsupported WTK_TARGET_PLATFORM='${WTK_TARGET_PLATFORM}'. Use 'host' or 'stm32'.")
endif()
