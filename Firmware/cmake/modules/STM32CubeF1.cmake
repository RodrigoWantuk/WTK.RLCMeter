set(WTK_STM32CUBEF1_COMPAT_VERSION "STM32CubeF1-v1.8.7")
set(WTK_STM32_CMSIS_CORE_SHA "afc5ca6af0a49232fde7eb4548dd0962d119ce14")
set(WTK_STM32_CMSIS_DEVICE_F1_SHA "c8e9a4a4f16b6d2cb2a2083cbe5161025280fb22")
set(WTK_STM32F1_HAL_DRIVER_SHA "fee494a92b5ad331f92ad21f76c66a5cb83773ee")

set(WTK_STM32_CMSIS_CORE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/st/cmsis_core"
    CACHE PATH "Path to the pinned ST CMSIS Core component"
)
set(WTK_STM32_CMSIS_DEVICE_F1_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/st/cmsis_device_f1"
    CACHE PATH "Path to the pinned ST CMSIS Device F1 component"
)
set(WTK_STM32F1_HAL_DRIVER_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/st/stm32f1xx_hal_driver"
    CACHE PATH "Path to the pinned ST STM32F1 HAL/LL driver component"
)

function(wtk_require_file file_path description)
    if(NOT EXISTS "${file_path}")
        message(FATAL_ERROR
            "Missing ${description}: ${file_path}\n"
            "Initialize official ST firmware submodules with:\n"
            "  git submodule update --init --recursive"
        )
    endif()
endfunction()

function(wtk_define_stm32cube_f1_interface)
    if(TARGET wtk_stm32cube_f1_headers)
        return()
    endif()

    set(cmsis_core_inc "${WTK_STM32_CMSIS_CORE_ROOT}/CMSIS/Core/Include")
    set(cmsis_device_inc "${WTK_STM32_CMSIS_DEVICE_F1_ROOT}/Include")
    set(hal_inc "${WTK_STM32F1_HAL_DRIVER_ROOT}/Inc")
    set(hal_legacy_inc "${WTK_STM32F1_HAL_DRIVER_ROOT}/Inc/Legacy")
    set(hal_src "${WTK_STM32F1_HAL_DRIVER_ROOT}/Src")

    wtk_require_file("${cmsis_core_inc}/core_cm3.h" "CMSIS Core Cortex-M3 header")
    wtk_require_file("${cmsis_device_inc}/stm32f103xb.h" "STM32F103xB CMSIS device header")
    wtk_require_file("${cmsis_device_inc}/system_stm32f1xx.h" "STM32F1 CMSIS system header")
    wtk_require_file("${cmsis_device_inc}/stm32f1xx.h" "STM32F1 CMSIS family header")
    wtk_require_file("${hal_inc}/stm32f1xx_hal.h" "STM32F1 HAL umbrella header")
    wtk_require_file("${hal_inc}/stm32f1xx_ll_gpio.h" "STM32F1 LL GPIO header")
    wtk_require_file("${hal_src}/stm32f1xx_hal.c" "STM32F1 HAL source directory")
    wtk_require_file("${hal_src}/stm32f1xx_ll_gpio.c" "STM32F1 LL source directory")

    add_library(wtk_stm32cube_f1_headers INTERFACE)
    target_include_directories(wtk_stm32cube_f1_headers SYSTEM INTERFACE
        "${cmsis_core_inc}"
        "${cmsis_device_inc}"
        "${hal_inc}"
        "${hal_legacy_inc}"
    )
    target_include_directories(wtk_stm32cube_f1_headers INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/config/stm32"
    )
    target_compile_definitions(wtk_stm32cube_f1_headers INTERFACE
        STM32F103xB
        USE_HAL_DRIVER
    )

    set(WTK_STM32F1_HAL_SOURCE_DIR "${hal_src}" CACHE INTERNAL "Pinned STM32F1 HAL/LL source directory")
endfunction()

function(wtk_configure_stm32cube_f1 target)
    wtk_define_stm32cube_f1_interface()
    target_link_libraries(${target} PRIVATE wtk_stm32cube_f1_headers)
endfunction()
