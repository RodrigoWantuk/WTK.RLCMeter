cmake_minimum_required(VERSION 3.25)
project(wtk_spi2_rx_repro C)

add_executable(spi2_rx_repro
    src/main.c
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/bsp/startup_stm32f103c8tx.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/bsp/system_stm32f1xx.c"
)
set_target_properties(spi2_rx_repro PROPERTIES OUTPUT_NAME "spi2-rx-repro" SUFFIX ".elf")
target_compile_definitions(spi2_rx_repro PRIVATE STM32F103xB)
target_compile_options(spi2_rx_repro PRIVATE -std=c17 -ffunction-sections -fdata-sections)
target_include_directories(spi2_rx_repro SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/st/cmsis_core/CMSIS/Core/Include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/st/cmsis_device_f1/Include"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/bsp/startup_stm32f103c8tx.c"
    PROPERTIES COMPILE_OPTIONS "-Wno-pedantic"
)
set(WTK_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/stm32/STM32F103C8Tx_FLASH.ld")
target_link_options(spi2_rx_repro PRIVATE
    "-T${WTK_LINKER_SCRIPT}"
    "-nostartfiles"
    "--specs=nano.specs"
    "--specs=nosys.specs"
    "-Wl,--gc-sections"
)
add_custom_command(TARGET spi2_rx_repro POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:spi2_rx_repro> "${CMAKE_CURRENT_BINARY_DIR}/spi2-rx-repro.hex"
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:spi2_rx_repro>
)
