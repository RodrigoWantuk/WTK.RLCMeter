cmake_minimum_required(VERSION 3.25)
project(ProductionAuditReproductions LANGUAGES C)
get_filename_component(DEFAULT_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(REPO_ROOT "${DEFAULT_REPO}" CACHE PATH "Repository containing the audited source")
set(FACTORY_HOST_BUILD "${REPO_ROOT}/Firmware/build/calibration-20261025/product-scheduling-host-factory"
    CACHE PATH "Existing Debug factory PRODUCT host build, built from the audited source")

add_executable(settings_contract settings_contract.c
    "${REPO_ROOT}/Firmware/src/app/app_settings_service.c"
    "${REPO_ROOT}/Firmware/src/storage/storage_layout.c"
    "${REPO_ROOT}/Firmware/src/ui/ui_text.c"
    "${REPO_ROOT}/Firmware/src/storage/storage_crc32.c")
target_include_directories(settings_contract PRIVATE "${REPO_ROOT}/Firmware/src")

add_executable(product_idle product_idle.c)
target_include_directories(product_idle PRIVATE
    "${REPO_ROOT}/Firmware/src" "${FACTORY_HOST_BUILD}/generated")
target_compile_definitions(product_idle PRIVATE
    AUDIT_SOURCE="${REPO_ROOT}/Firmware/tests/app/test_app_product.c")
if(NOT MSVC)
    message(FATAL_ERROR "The recorded product_idle reproduction uses an MSVC Debug host library")
endif()
target_link_libraries(product_idle PRIVATE "${FACTORY_HOST_BUILD}/Debug/wtk_firmware_common.lib")
foreach(target settings_contract product_idle)
    target_compile_features(${target} PRIVATE c_std_17)
    target_compile_options(${target} PRIVATE /W4 /WX /UNDEBUG)
endforeach()
enable_testing()
# These assert the audited defects, not corrected behavior or production readiness.
add_test(NAME audit_confirms_settings_defects COMMAND settings_contract)
add_test(NAME audit_confirms_idle_defect COMMAND product_idle)
