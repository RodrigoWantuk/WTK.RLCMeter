#ifndef WTK_UI_PRODUCT_H
#define WTK_UI_PRODUCT_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/ili9341.h"
#include "measurement/measurement_engine.h"

typedef enum
{
    UI_PRODUCT_STATE_STARTUP = 0,
    UI_PRODUCT_STATE_SELF_TEST,
    UI_PRODUCT_STATE_CALIBRATION_CHECK,
    UI_PRODUCT_STATE_CALIBRATION_REQUIRED,
    UI_PRODUCT_STATE_READY,
    UI_PRODUCT_STATE_MEASURING,
    UI_PRODUCT_STATE_RESULT,
    UI_PRODUCT_STATE_SAFETY_BLOCKED,
    UI_PRODUCT_STATE_FAULT,
} ui_product_state_t;

typedef enum
{
    UI_PRODUCT_PAGE_PRIMARY = 0,
    UI_PRODUCT_PAGE_DETAILS,
    UI_PRODUCT_PAGE_COUNT,
} ui_product_page_t;

typedef enum
{
    UI_PRODUCT_CAL_UNKNOWN = 0,
    UI_PRODUCT_CAL_ACTIVE_VALID,
    UI_PRODUCT_CAL_REQUIRED,
    UI_PRODUCT_CAL_STORAGE_ERROR,
} ui_product_calibration_state_t;

typedef enum
{
    UI_PRODUCT_BLOCK_NONE = 0,
    UI_PRODUCT_BLOCK_CHARGER,
    UI_PRODUCT_BLOCK_RESIDUAL,
    UI_PRODUCT_BLOCK_SENSOR,
    UI_PRODUCT_BLOCK_SUPPLY,
    UI_PRODUCT_BLOCK_RANGE,
    UI_PRODUCT_BLOCK_FAULT,
} ui_product_blocker_t;

typedef enum
{
    UI_PRODUCT_BATTERY_UNKNOWN = 0,
    UI_PRODUCT_BATTERY_OK,
    UI_PRODUCT_BATTERY_LOW,
    UI_PRODUCT_BATTERY_CRITICAL,
} ui_product_battery_t;

typedef struct
{
    ui_product_state_t state;
    ui_product_page_t page;
    ui_product_calibration_state_t calibration_status;
    ui_product_blocker_t safety_blocker;
    ui_product_battery_t battery_state;
    uint8_t measurement_state;
    measurement_session_result_t measurement_result;
    bool has_measurement_result;
    bool measurement_result_partial;
    bool storage_unavailable;
    bool display_ready;
    bool display_fault;
    bool menu_not_implemented;
    uint32_t safety_fault_mask;
    uint32_t session_sequence;
    uint32_t generation;
} ui_product_view_t;

typedef struct
{
    ili9341_fill_t clear_fill;
    ui_product_view_t pending;
    uint32_t rendered_generation;
    bool active;
    bool clear_started;
} ui_product_t;

void ui_product_init(ui_product_t *ui);
void ui_product_request(ui_product_t *ui, const ui_product_view_t *view);
bsp_status_t ui_product_step(ui_product_t *ui, const ili9341_t *display, bool quiet);
uint32_t ui_product_context_size_bytes(void);
const char *ui_product_state_string(ui_product_state_t state);

#endif
