#ifndef WTK_APP_BRINGUP_CONSOLE_H
#define WTK_APP_BRINGUP_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

#include "app/app_calibration_service.h"
#include "app/app_calibration_session.h"
#include "app/app_io_workspace.h"
#include "app/app_safety_fault.h"
#include "drivers/ili9341.h"
#include "drivers/w25q.h"
#include "hardware/hw_aux_sensors.h"
#include "hardware/hw_charger.h"
#include "hardware/hw_k1.h"
#include "hardware/hw_metrology_measure.h"
#include "hardware/hw_metrology_session.h"
#include "hardware/hw_range.h"
#include "hardware/hw_safety.h"
#include "measurement/measurement_calibration_store.h"
#include "wtk_build_config.h"

enum
{
    APP_BRINGUP_CONSOLE_LINE_CAPACITY = 64u,
    APP_BRINGUP_FLASH_TEST_SIZE = 64u,
};

typedef enum
{
    APP_BRINGUP_FLASH_TEST_IDLE = 0,
    APP_BRINGUP_FLASH_TEST_ERASE_START,
    APP_BRINGUP_FLASH_TEST_ERASE_WAIT,
    APP_BRINGUP_FLASH_TEST_VERIFY_ERASE,
    APP_BRINGUP_FLASH_TEST_PROGRAM_START,
    APP_BRINGUP_FLASH_TEST_PROGRAM_WAIT,
    APP_BRINGUP_FLASH_TEST_READBACK,
    APP_BRINGUP_FLASH_TEST_VERIFY,
    APP_BRINGUP_FLASH_TEST_CLEANUP_ERASE_START,
    APP_BRINGUP_FLASH_TEST_CLEANUP_ERASE_WAIT,
    APP_BRINGUP_FLASH_TEST_COMPLETE,
    APP_BRINGUP_FLASH_TEST_ERROR,
} app_bringup_flash_test_state_t;

typedef enum
{
    APP_BRINGUP_METROLOGY_DUMP_NONE = 0,
    APP_BRINGUP_METROLOGY_DUMP_CAPTURE,
    APP_BRINGUP_METROLOGY_DUMP_MEASURE,
} app_bringup_metrology_dump_source_t;

typedef struct
{
#if WTK_ENABLE_BRINGUP_CONSOLE
    char line[APP_BRINGUP_CONSOLE_LINE_CAPACITY];
    uint8_t line_length;
    app_bringup_flash_test_state_t flash_test_state;
    uint32_t test_sector_address;
    uint8_t pattern[APP_BRINGUP_FLASH_TEST_SIZE];
    uint8_t readback[APP_BRINGUP_FLASH_TEST_SIZE];
    bool busy_observed;
    hw_metrology_session_t session;
    hw_metrology_measure_t measure;
    app_calibration_session_t cal_session;
    app_calibration_service_t *cal_service;
    app_io_workspace_t *workspace;
    app_bringup_metrology_dump_source_t dump_source;
    uint16_t dump_row;
    bool dump_active;
    uint16_t ccr_table[HW_EXCITATION_LUT_POINTS];
    hw_range_t *range_ref;
    hw_k1_t *k1_ref;
    hw_aux_sensors_t *sensors_ref;
    hw_charger_t *charger_ref;
    app_safety_fault_latch_t *faults_ref;
#else
    uint8_t unused;
#endif
} app_bringup_console_t;

void app_bringup_console_init(app_bringup_console_t *console);
void app_bringup_console_attach_calibration_service(app_bringup_console_t *console,
                                                app_calibration_service_t *service);
void app_bringup_console_attach_workspace(app_bringup_console_t *console,
                                          app_io_workspace_t *workspace);
void app_bringup_console_step(app_bringup_console_t *console,
                          w25q_device_t *flash,
                          ili9341_t *display,
                          hw_range_t *range,
                          hw_charger_t *charger,
                          hw_aux_sensors_t *sensors,
                          hw_k1_t *k1,
                          const hw_safety_result_t *safety,
                          app_safety_fault_latch_t *faults,
                          uint32_t now_ms);
bool app_bringup_console_flash_busy(const app_bringup_console_t *console);
bool app_bringup_console_capture_busy(const app_bringup_console_t *console);
uint32_t app_bringup_console_context_size_bytes(void);

#endif
