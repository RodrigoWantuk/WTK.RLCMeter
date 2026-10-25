set(WTK_NTC_FACTORY_PROFILE "" CACHE FILEPATH "Optional versioned per-unit engineering NTC profile")
set(WTK_NTC_FACTORY_CALIBRATION_VALUE 0)
set(WTK_NTC_FACTORY_UID "")
set(WTK_NTC_PROFILE_SHA256 "")
if(WTK_NTC_FACTORY_PROFILE)
    if(NOT Python3_Interpreter_FOUND)
        message(FATAL_ERROR "Python 3 is required to validate an NTC factory profile")
    endif()
    get_filename_component(WTK_NTC_FACTORY_PROFILE_ABSOLUTE "${WTK_NTC_FACTORY_PROFILE}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${WTK_NTC_FACTORY_PROFILE_ABSOLUTE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/ntc_factory_profile.py")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/ntc_factory_profile.py"
        --profile "${WTK_NTC_FACTORY_PROFILE_ABSOLUTE}"
        --header "${CMAKE_CURRENT_BINARY_DIR}/generated/wtk_ntc_factory_lut.h"
        --metadata "${CMAKE_CURRENT_BINARY_DIR}/generated/wtk_ntc_factory_profile.json"
        RESULT_VARIABLE WTK_NTC_GENERATE_RESULT ERROR_VARIABLE WTK_NTC_GENERATE_ERROR)
    if(NOT WTK_NTC_GENERATE_RESULT EQUAL 0)
        message(FATAL_ERROR "Invalid NTC factory profile: ${WTK_NTC_GENERATE_ERROR}")
    endif()
    file(READ "${CMAKE_CURRENT_BINARY_DIR}/generated/wtk_ntc_factory_profile.json" WTK_NTC_METADATA)
    string(JSON WTK_NTC_FACTORY_UID GET "${WTK_NTC_METADATA}" uid)
    string(JSON WTK_NTC_PROFILE_SHA256 GET "${WTK_NTC_METADATA}" profile_sha256)
    set(WTK_NTC_FACTORY_CALIBRATION_VALUE 1)
    message(STATUS "Engineering NTC factory profile for UID ${WTK_NTC_FACTORY_UID}; unqualified")
endif()
