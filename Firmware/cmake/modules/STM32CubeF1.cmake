set(WTK_STM32CUBEF1_VERSION "v1.8.6" CACHE STRING "Pinned STM32CubeF1 upstream version intended for CMSIS/HAL/LL integration")
set(WTK_STM32CUBEF1_ROOT "" CACHE PATH "Path to a checked-out or vendored STM32CubeF1 package")

function(wtk_configure_stm32cube_f1 target)
    if(WTK_STM32CUBEF1_ROOT)
        set(cmsis_core "${WTK_STM32CUBEF1_ROOT}/Drivers/CMSIS/Include")
        set(cmsis_device "${WTK_STM32CUBEF1_ROOT}/Drivers/CMSIS/Device/ST/STM32F1xx/Include")
        set(hal_inc "${WTK_STM32CUBEF1_ROOT}/Drivers/STM32F1xx_HAL_Driver/Inc")

        if(EXISTS "${cmsis_core}/core_cm3.h" AND EXISTS "${cmsis_device}/stm32f103xb.h")
            target_include_directories(${target} SYSTEM PRIVATE
                "${cmsis_core}"
                "${cmsis_device}"
            )
            target_compile_definitions(${target} PRIVATE STM32F103xB)
        else()
            message(FATAL_ERROR "WTK_STM32CUBEF1_ROOT does not contain the expected CMSIS STM32F1 headers.")
        endif()

        if(EXISTS "${hal_inc}/stm32f1xx_hal.h")
            target_include_directories(${target} SYSTEM PRIVATE "${hal_inc}")
        endif()
    else()
        message(STATUS "WTK_STM32CUBEF1_ROOT is not set. Phase 01 minimal STM32 link-smoke target will build without HAL peripheral sources.")
    endif()
endfunction()
