#ifndef WTK_UI_PRODUCT_H
#define WTK_UI_PRODUCT_H

#include <stdbool.h>
#include <stdint.h>

#include "drivers/ili9341.h"
#include "measurement/measurement_engine.h"
#include "ui/ui_fallback_renderer.h"

typedef enum
{
    UI_PRODUCT_STATE_STARTUP = 0,
    UI_PRODUCT_STATE_SELF_TEST,
    UI_PRODUCT_STATE_CALIBRATION_CHECK,
    UI_PRODUCT_STATE_CALIBRATION_REQUIRED,
    UI_PRODUCT_STATE_READY,
    UI_PRODUCT_STATE_MENU,
    UI_PRODUCT_STATE_CALIBRATION_STATUS,
    UI_PRODUCT_STATE_CALIBRATION_WIZARD,
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
    UI_PRODUCT_RENDER_IDLE = 0,
    UI_PRODUCT_RENDER_CLEAR,
    UI_PRODUCT_RENDER_TEXT,
} ui_product_render_state_t;

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

typedef enum
{
    UI_PRODUCT_WIZARD_IDLE = 0,
    UI_PRODUCT_WIZARD_INTRO,
    UI_PRODUCT_WIZARD_WAIT_OPEN,
    UI_PRODUCT_WIZARD_CAPTURE_OPEN,
    UI_PRODUCT_WIZARD_WAIT_SHORT,
    UI_PRODUCT_WIZARD_CAPTURE_SHORT,
    UI_PRODUCT_WIZARD_WAIT_LOAD,
    UI_PRODUCT_WIZARD_CAPTURE_LOAD,
    UI_PRODUCT_WIZARD_RANGE_COMPLETE,
    UI_PRODUCT_WIZARD_CONFIRM_SAVE,
    UI_PRODUCT_WIZARD_COMMITTING,
    UI_PRODUCT_WIZARD_COMPLETE,
    UI_PRODUCT_WIZARD_FAILED,
    UI_PRODUCT_WIZARD_SAFETY_BLOCKED,
    UI_PRODUCT_WIZARD_CANCELING,
    UI_PRODUCT_WIZARD_CANCELED,
} ui_product_wizard_state_t;

typedef enum
{
    UI_PRODUCT_WIZARD_STANDARD_OPEN = 0,
    UI_PRODUCT_WIZARD_STANDARD_SHORT,
    UI_PRODUCT_WIZARD_STANDARD_LOAD,
} ui_product_wizard_standard_t;

typedef struct
{
    measurement_auto_status_t status;
    measurement_interpretation_t interpretation;
    measurement_confidence_class_t confidence;
    measurement_quality_class_t quality;
    measurement_qualification_t qualification;
    hw_excitation_freq_t frequency;
    hw_excitation_amp_t amplitude;
    float resistance_ohms;
    float reactance_ohms;
    float magnitude_ohms;
    float phase_rad;
    float capacitance_f;
    float inductance_h;
    uint8_t attempt_count;
    uint8_t primary_attempt_index;
    bool derived_valid;
    bool capacitance_valid;
    bool inductance_valid;
} ui_product_measurement_t;

typedef struct
{
    uint8_t selected_index;
    uint8_t item_count;
} ui_product_menu_t;

typedef struct
{
    uint8_t state;
    uint8_t mode;
    uint8_t standard;
    uint8_t error;
    uint8_t workflow_result;
    uint8_t solver_status;
    hw_range_id_t range_id;
    hw_excitation_freq_t frequency;
    hw_excitation_amp_t amplitude;
    uint8_t range_index;
    uint8_t range_count;
    uint8_t condition_index;
    uint8_t condition_count;
    uint8_t solved_count;
    uint8_t total_conditions;
    uint8_t accepted;
    uint8_t attempts;
    bool mandatory;
} ui_product_wizard_t;

typedef struct
{
    ui_product_state_t state;
    ui_product_page_t page;
    ui_product_calibration_state_t calibration_status;
    ui_product_blocker_t safety_blocker;
    ui_product_battery_t battery_state;
    uint8_t measurement_state;
    ui_product_measurement_t measurement_result;
    ui_product_menu_t menu;
    ui_product_wizard_t wizard;
    bool has_measurement_result;
    bool measurement_result_partial;
    bool storage_unavailable;
    bool display_ready;
    bool display_fault;
    bool calibration_active_valid;
    uint32_t calibration_sequence;
    uint32_t safety_fault_mask;
    uint32_t session_sequence;
    uint32_t generation;
} ui_product_view_t;

typedef struct
{
    ili9341_fill_t clear_fill;
    ui_product_view_t pending;
    ui_product_view_t rendered;
    ui_product_view_t rendering;
    ui_fallback_text_op_t text_op;
    uint32_t rendered_generation;
    uint8_t line_index;
    ui_product_render_state_t render_state;
    bool active;
    bool have_rendered;
    bool clear_started;
} ui_product_t;

void ui_product_init(ui_product_t *ui);
void ui_product_request(ui_product_t *ui, const ui_product_view_t *view);
bsp_status_t ui_product_step(ui_product_t *ui, const ili9341_t *display, bool quiet);
uint32_t ui_product_context_size_bytes(void);
const char *ui_product_state_string(ui_product_state_t state);

#endif
