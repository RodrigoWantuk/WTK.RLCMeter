#include "app/app_shell.h"

#include "app/app_bringup_console.h"
#include "app/app_calibration_service.h"
#include "app/app_flash_access.h"
#include "app/app_io_workspace.h"
#if !WTK_ENABLE_BRINGUP_CONSOLE && WTK_ENABLE_PRODUCT_RESOURCE_UPDATE
#include "app/app_pc_link_protocol.h"
#endif
#include "app/app_product.h"
#if !WTK_ENABLE_BRINGUP_CONSOLE && WTK_ENABLE_PRODUCT_RESOURCE_UPDATE
#include "app/app_resource_update.h"
#endif
#include "app/app_safety_fault.h"
#include "app/app_settings_service.h"
#include "bsp/bsp_adc.h"
#include "bsp/bsp_clock.h"
#include "bsp/bsp_diagnostics.h"
#include "bsp/bsp_excitation.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_metrology_adc.h"
#include "bsp/bsp_quiet.h"
#include "bsp/bsp_reset.h"
#include "bsp/bsp_status.h"
#include "bsp/bsp_time.h"
#include "bsp/bsp_uart.h"
#include "bsp/bsp_watchdog.h"
#include "drivers/buttons.h"
#include "drivers/ili9341.h"
#include "drivers/spi_bus.h"
#include "drivers/w25q.h"
#include "hardware/hw_backlight.h"
#include "hardware/hw_aux_sensors.h"
#include "hardware/hw_buzzer.h"
#include "hardware/hw_charger.h"
#include "hardware/hw_excitation.h"
#include "hardware/hw_metrology_measure.h"
#include "hardware/hw_k2.h"
#include "hardware/hw_metrology_clock.h"
#include "hardware/hw_peripherals.h"
#include "hardware/hw_range.h"
#include "hardware/hw_safety.h"
#include "ui/ui_fallback_renderer.h"
#include "ui/ui_product.h"
#include "wtk_build_config.h"
#if !WTK_ENABLE_BRINGUP_CONSOLE
#include "storage/app_settings_w25q_adapter.h"
#endif
#include "storage/measurement_cal_w25q_adapter.h"
#include "storage/resource_store.h"
#include "storage/resource_w25q_adapter.h"
#include "storage/storage_layout.h"
#include "ui/ui_font.h"
#if WTK_ENABLE_PRODUCT_RICH_IMAGES
#include "ui/ui_image.h"
#endif
#include "ui/ui_text_catalog.h"

#if WTK_DIAGNOSTIC_LOG_LEVEL_DEFAULT >= 4u
#define APP_VERBOSE_DIAG_TEXT(key, value) bsp_diagnostics_write_key_value_text((key), (value))
#define APP_VERBOSE_DIAG_U32(key, value) bsp_diagnostics_write_key_value_u32((key), (value))
#define APP_VERBOSE_DIAG_HEX8(key, value) bsp_diagnostics_write_key_value_hex8((key), (value))
#else
#define APP_VERBOSE_DIAG_TEXT(key, value) ((void)0)
#define APP_VERBOSE_DIAG_U32(key, value) ((void)0)
#define APP_VERBOSE_DIAG_HEX8(key, value) ((void)0)
#endif

#if defined(__GNUC__)
#define APP_NOINLINE __attribute__((noinline))
#else
#define APP_NOINLINE
#endif

typedef enum
{
    APP_SAFETY_SAFE_CHECK = 0,
    APP_SAFETY_WAIT_SAFE,
    APP_SAFETY_READY,
} app_safety_state_t;

static buttons_t g_buttons;
static ili9341_t g_display;
static w25q_device_t g_flash;
static hw_k1_t g_k1;
static hw_k2_t g_k2;
static hw_range_t g_range;
static hw_charger_t g_charger;
static hw_aux_sensors_t g_aux_sensors;
static app_safety_fault_latch_t g_safety_faults;
static hw_safety_result_t g_safety_result;
static app_calibration_service_t g_calibration_service;
static app_io_workspace_t g_io_workspace;
#if WTK_ENABLE_BRINGUP_CONSOLE
static app_bringup_console_t g_bringup_console;
static bool g_display_ready_reported = false;
#else
static app_settings_service_t g_settings_service;
static app_product_t g_product;
static ui_product_t g_product_ui;
static hw_metrology_measure_t g_product_measure;
static resource_catalog_t g_resource_catalog;
static ui_text_catalog_t g_text_catalog;
static ui_font_catalog_t g_font_catalog;
#if WTK_ENABLE_PRODUCT_RICH_IMAGES
static ui_image_catalog_t g_image_catalog;
#endif
static resource_w25q_reader_t g_resource_reader;
static resource_status_t g_resource_status = RESOURCE_STATUS_MISSING;
#if WTK_ENABLE_PRODUCT_RESOURCE_UPDATE
static app_resource_update_t g_resource_update;
static uint8_t g_pc_link_frame[APP_PC_LINK_HEADER_SIZE + APP_PC_LINK_MAX_PAYLOAD_BYTES];
static uint16_t g_pc_link_received = 0u;
static uint16_t g_pc_link_expected = APP_PC_LINK_HEADER_SIZE;
static uint16_t g_pc_link_pending_sequence = 0u;
static app_pc_link_status_t g_pc_link_pending_protocol_status = APP_PC_LINK_STATUS_OK;
static app_resource_update_status_t g_pc_link_pending_update_status = APP_RESOURCE_UPDATE_STATUS_OK;
static bool g_pc_link_frame_ready = false;
static bool g_pc_link_ack_deferred = false;
static bool g_resource_update_configured = false;
static bool g_resource_update_workspace_acquired = false;
#endif
static uint16_t g_product_ccr_table[HW_EXCITATION_LUT_POINTS];
static uint32_t g_product_output_tone_sequence = 0u;
static uint8_t g_product_output_backlight_percent = UINT8_MAX;
static bool g_product_output_sound_valid = false;
static bool g_product_output_sound_enabled = true;
static bool g_product_display_fault = false;
#endif
static bool g_flash_fallback_drawn = false;
static bool g_safety_transition_reported = false;
static app_safety_state_t g_reported_safety_state = APP_SAFETY_SAFE_CHECK;
static hw_safety_primary_blocker_t g_reported_primary_blocker = HW_SAFETY_BLOCKED_SENSOR_INVALID;
static bsp_status_t g_clock_status = BSP_STATUS_ERROR;

static app_safety_state_t g_safety_state = APP_SAFETY_SAFE_CHECK;

static void app_latch_fault(uint32_t fault_mask);

static bsp_status_t app_write_k1_cmd(bool high, void *user_data)
{
    (void)user_data;
    return bsp_gpio_write_output(BSP_GPIO_OUTPUT_K1_CMD, high);
}

static bsp_status_t app_write_k2_cmd(bool high, void *user_data)
{
    (void)user_data;
    return bsp_gpio_write_output(BSP_GPIO_OUTPUT_K2_CMD, high);
}

static bsp_status_t app_write_range_enable(bool high, void *user_data)
{
    (void)user_data;
    return bsp_gpio_write_output(BSP_GPIO_OUTPUT_RANGE_EN, high);
}

static bsp_status_t app_write_range_address(uint8_t address, void *user_data)
{
    (void)user_data;
    bsp_status_t status = bsp_gpio_write_output(BSP_GPIO_OUTPUT_RANGE_A0, (address & 0x01u) != 0u);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    status = bsp_gpio_write_output(BSP_GPIO_OUTPUT_RANGE_A1, (address & 0x02u) != 0u);
    if (status != BSP_STATUS_OK)
    {
        return status;
    }
    return bsp_gpio_write_output(BSP_GPIO_OUTPUT_RANGE_A2, (address & 0x04u) != 0u);
}

static bsp_status_t app_read_charger_gpio(bool *high, void *user_data)
{
    (void)user_data;
    return bsp_gpio_read_input(BSP_GPIO_INPUT_CHARGER_DETECT, high);
}

static bsp_status_t app_adc_start(bsp_adc_channel_t channel, uint32_t now_ms, void *user_data)
{
    (void)user_data;
    return bsp_adc_start(channel, now_ms);
}

static bsp_status_t app_adc_poll(uint16_t *raw, uint32_t now_ms, void *user_data)
{
    (void)user_data;
    return bsp_adc_poll(raw, now_ms);
}

static void app_adc_cancel(void *user_data)
{
    (void)user_data;
    bsp_adc_cancel();
}

#if !WTK_ENABLE_BRINGUP_CONSOLE
static app_flash_access_snapshot_t product_flash_access_snapshot(void *user)
{
    (void)user;
    return (app_flash_access_snapshot_t){
        .quiet = hw_peripherals_quiet_requested(),
        .calibration_mutation = app_calibration_service_busy(&g_calibration_service),
        .settings_mutation = app_settings_service_busy(&g_settings_service),
#if WTK_ENABLE_PRODUCT_RESOURCE_UPDATE
        .resource_mutation = app_resource_update_active(&g_resource_update),
#else
        .resource_mutation = false,
#endif
    };
}

static APP_NOINLINE resource_status_t product_mount_resource_pack(void)
{
    if (!g_flash.detected)
    {
        return RESOURCE_STATUS_MISSING;
    }
    storage_partition_t resource_partition;
    if (!storage_layout_partition(g_flash.part.capacity_bytes,
                                  STORAGE_PARTITION_RESOURCE_PACK,
                                  &resource_partition))
    {
        return RESOURCE_STATUS_CORRUPT;
    }

    g_resource_reader = (resource_w25q_reader_t){
        .flash = &g_flash,
        .policy_snapshot = product_flash_access_snapshot,
        .policy_user = NULL,
    };
    const resource_catalog_io_t resource_io = resource_w25q_catalog_io(&g_resource_reader);
    resource_status_t status = resource_catalog_mount(&g_resource_catalog,
                                                      &resource_io,
                                                      resource_partition.start,
                                                      resource_partition.size);
    if (status == RESOURCE_STATUS_OK)
    {
        status = ui_text_catalog_validate_required_languages(&g_resource_catalog);
    }
    if (status == RESOURCE_STATUS_OK)
    {
        status = ui_font_catalog_mount(&g_font_catalog, &g_resource_catalog);
    }
#if WTK_ENABLE_PRODUCT_RICH_IMAGES
    if (status == RESOURCE_STATUS_OK)
    {
        const resource_status_t image_status =
            ui_image_catalog_mount(&g_image_catalog, &g_resource_catalog);
        if ((image_status != RESOURCE_STATUS_OK) &&
            (image_status != RESOURCE_STATUS_MISSING))
        {
            status = image_status;
        }
    }
#endif
    if (status == RESOURCE_STATUS_OK)
    {
        const uint8_t language_id = app_settings_service_current(&g_settings_service)->language_id;
        status = ui_text_catalog_select_language(&g_text_catalog,
                                                 &g_resource_catalog,
                                                 language_id);
    }
    return status;
}

#if WTK_ENABLE_PRODUCT_RESOURCE_UPDATE
static bsp_status_t product_resource_erase_start(uint32_t address, uint32_t now_ms, void *user)
{
    (void)user;
    const w25q_status_t status = w25q_device_sector_erase_start(&g_flash, address, now_ms);
    if (status == W25Q_STATUS_BUSY)
    {
        return BSP_STATUS_BUSY;
    }
    return (status == W25Q_STATUS_OK) ? BSP_STATUS_OK : BSP_STATUS_ERROR;
}

static bsp_status_t product_resource_program_start(uint32_t address,
                                                   const void *src,
                                                   size_t size,
                                                   uint32_t now_ms,
                                                   void *user)
{
    (void)user;
    const w25q_status_t status = w25q_device_page_program_start(&g_flash, address, src, size, now_ms);
    if (status == W25Q_STATUS_BUSY)
    {
        return BSP_STATUS_BUSY;
    }
    return (status == W25Q_STATUS_OK) ? BSP_STATUS_OK : BSP_STATUS_ERROR;
}

static bsp_status_t product_resource_poll(uint32_t now_ms, void *user)
{
    (void)user;
    const w25q_status_t status = w25q_device_poll(&g_flash, now_ms);
    if (status == W25Q_STATUS_BUSY)
    {
        return BSP_STATUS_BUSY;
    }
    return (status == W25Q_STATUS_OK) ? BSP_STATUS_OK : BSP_STATUS_ERROR;
}

static uint16_t app_read_le16(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static uint32_t app_read_le32(const uint8_t *src)
{
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static void app_write_le16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void product_pc_link_reset_rx(void)
{
    g_pc_link_received = 0u;
    g_pc_link_expected = APP_PC_LINK_HEADER_SIZE;
    g_pc_link_frame_ready = false;
}

static APP_NOINLINE void product_pc_link_send_status(uint16_t sequence,
                                                     app_pc_link_status_t protocol_status,
                                                     app_resource_update_status_t update_status)
{
    uint8_t payload[8] = {0};
    uint8_t header[APP_PC_LINK_HEADER_SIZE];
    app_write_le16(&payload[0], sequence);
    app_write_le16(&payload[2], (uint16_t)protocol_status);
    app_write_le16(&payload[4], (uint16_t)update_status);
    app_write_le16(&payload[6], (uint16_t)app_resource_update_state(&g_resource_update));
    app_pc_link_encode_header(header,
                              APP_PC_LINK_FRAME_STATUS,
                              0u,
                              sequence,
                              payload,
                              (uint16_t)sizeof(payload));
    (void)bsp_uart_write((const char *)header, sizeof(header));
    (void)bsp_uart_write((const char *)payload, sizeof(payload));
}

static void product_resource_update_release_workspace(void)
{
    if (g_resource_update_workspace_acquired)
    {
        (void)app_io_workspace_release(&g_io_workspace, APP_IO_WORKSPACE_OWNER_RESOURCE_UPDATE);
        g_resource_update_workspace_acquired = false;
    }
}

static APP_NOINLINE app_resource_update_status_t product_resource_update_begin(const app_pc_link_frame_t *frame,
                                                                               uint32_t now_ms)
{
    if (!g_flash.detected)
    {
        return APP_RESOURCE_UPDATE_STATUS_FLASH;
    }
    const app_flash_access_snapshot_t access = product_flash_access_snapshot(NULL);
    if (!app_flash_access_allowed(&access, APP_FLASH_ACCESS_RESOURCE_MUTATION))
    {
        return APP_RESOURCE_UPDATE_STATUS_BUSY;
    }

    storage_partition_t resource_partition;
    if (!storage_layout_partition(g_flash.part.capacity_bytes,
                                  STORAGE_PARTITION_RESOURCE_PACK,
                                  &resource_partition))
    {
        return APP_RESOURCE_UPDATE_STATUS_OUT_OF_RANGE;
    }
    if (app_io_workspace_acquire(&g_io_workspace,
                                 APP_IO_WORKSPACE_OWNER_RESOURCE_UPDATE) != BSP_STATUS_OK)
    {
        return APP_RESOURCE_UPDATE_STATUS_BUSY;
    }
    g_resource_update_workspace_acquired = true;
    const app_resource_update_io_t io = {
        .erase_start = product_resource_erase_start,
        .program_start = product_resource_program_start,
        .poll = product_resource_poll,
        .user = NULL,
    };
    app_resource_update_init(&g_resource_update,
                             &io,
                             &resource_partition,
                             app_io_workspace_resource_update_frame(&g_io_workspace),
                             app_io_workspace_resource_update_frame_bytes());
    g_resource_update_configured = true;
    g_resource_status = RESOURCE_STATUS_DEFERRED;
    return app_resource_update_accept_frame(&g_resource_update, frame, now_ms);
}

static APP_NOINLINE app_resource_update_status_t product_pc_link_process_frame(const app_pc_link_frame_t *frame,
                                                                               uint32_t now_ms)
{
    if (frame == NULL)
    {
        g_pc_link_pending_protocol_status = APP_PC_LINK_STATUS_INVALID_ARG;
        return APP_RESOURCE_UPDATE_STATUS_INVALID_ARG;
    }
    if (g_product.view.state != UI_PRODUCT_STATE_PC_LINK_STATUS)
    {
        g_pc_link_pending_protocol_status = APP_PC_LINK_STATUS_UNEXPECTED_FRAME;
        return APP_RESOURCE_UPDATE_STATUS_PROTOCOL;
    }
    if (frame->type == APP_PC_LINK_FRAME_RESOURCE_BEGIN)
    {
        g_pc_link_pending_protocol_status = APP_PC_LINK_STATUS_OK;
        return product_resource_update_begin(frame, now_ms);
    }
    if (!g_resource_update_configured)
    {
        g_pc_link_pending_protocol_status = APP_PC_LINK_STATUS_NOT_ACTIVE;
        return APP_RESOURCE_UPDATE_STATUS_PROTOCOL;
    }

    const app_resource_update_status_t status =
        app_resource_update_accept_frame(&g_resource_update, frame, now_ms);
    g_pc_link_pending_protocol_status = app_resource_update_last_protocol_status(&g_resource_update);
    if (frame->type == APP_PC_LINK_FRAME_ABORT)
    {
        product_resource_update_release_workspace();
        g_resource_update_configured = false;
        g_resource_status = product_mount_resource_pack();
    }
    return status;
}

static void product_pc_link_finish_update_if_terminal(void)
{
    const app_resource_update_state_t state = app_resource_update_state(&g_resource_update);
    if (state == APP_RESOURCE_UPDATE_COMPLETE)
    {
        product_resource_update_release_workspace();
        g_resource_update_configured = false;
        g_resource_status = product_mount_resource_pack();
    }
    else if (state == APP_RESOURCE_UPDATE_ERROR)
    {
        product_resource_update_release_workspace();
        g_resource_update_configured = false;
        g_resource_status = RESOURCE_STATUS_CORRUPT;
    }
}

static APP_NOINLINE void product_pc_link_step_update(uint32_t now_ms)
{
    if (!g_resource_update_configured)
    {
        return;
    }
    const app_resource_update_status_t status = app_resource_update_step(&g_resource_update, now_ms);
    if ((status != APP_RESOURCE_UPDATE_STATUS_OK) &&
        (status != APP_RESOURCE_UPDATE_STATUS_BUSY) &&
        (status != APP_RESOURCE_UPDATE_STATUS_COMPLETE))
    {
        g_pc_link_pending_update_status = status;
    }
    if (g_pc_link_ack_deferred &&
        !app_resource_update_busy(&g_resource_update))
    {
        if (g_pc_link_pending_update_status == APP_RESOURCE_UPDATE_STATUS_OK)
        {
            g_pc_link_pending_update_status =
                app_resource_update_complete(&g_resource_update) ?
                    APP_RESOURCE_UPDATE_STATUS_COMPLETE :
                    app_resource_update_last_error(&g_resource_update);
        }
        product_pc_link_send_status(g_pc_link_pending_sequence,
                                    g_pc_link_pending_protocol_status,
                                    g_pc_link_pending_update_status);
        g_pc_link_ack_deferred = false;
        product_pc_link_finish_update_if_terminal();
    }
}

static APP_NOINLINE void product_pc_link_decode_ready_frame(uint32_t now_ms)
{
    app_pc_link_frame_t frame;
    app_pc_link_status_t pc_status =
        app_pc_link_decode_frame(g_pc_link_frame, g_pc_link_expected, &frame);
    uint16_t sequence = 0u;
    if (g_pc_link_expected >= APP_PC_LINK_HEADER_SIZE)
    {
        sequence = app_read_le16(&g_pc_link_frame[8]);
    }
    app_resource_update_status_t update_status = APP_RESOURCE_UPDATE_STATUS_PROTOCOL;
    if (pc_status == APP_PC_LINK_STATUS_OK)
    {
        update_status = product_pc_link_process_frame(&frame, now_ms);
        pc_status = g_pc_link_pending_protocol_status;
    }

    g_pc_link_pending_sequence = sequence;
    g_pc_link_pending_protocol_status = pc_status;
    g_pc_link_pending_update_status = update_status;
    product_pc_link_reset_rx();

    if ((update_status == APP_RESOURCE_UPDATE_STATUS_OK) &&
        app_resource_update_busy(&g_resource_update))
    {
        g_pc_link_ack_deferred = true;
        return;
    }

    product_pc_link_send_status(sequence, pc_status, update_status);
    product_pc_link_finish_update_if_terminal();
}

static APP_NOINLINE void product_pc_link_step_rx(uint32_t now_ms)
{
    if (g_pc_link_ack_deferred || g_pc_link_frame_ready)
    {
        return;
    }
    for (uint8_t i = 0u; i < 16u; i++)
    {
        uint8_t byte = 0u;
        const bsp_status_t status = bsp_uart_try_read_byte(&byte);
        if (status != BSP_STATUS_OK)
        {
            break;
        }
        if (g_pc_link_received < sizeof(g_pc_link_frame))
        {
            g_pc_link_frame[g_pc_link_received] = byte;
            g_pc_link_received++;
        }
        if (g_pc_link_received == APP_PC_LINK_HEADER_SIZE)
        {
            const uint16_t payload_length = app_read_le16(&g_pc_link_frame[10]);
            if ((app_read_le32(&g_pc_link_frame[0]) != APP_PC_LINK_MAGIC) ||
                (g_pc_link_frame[4] != APP_PC_LINK_VERSION) ||
                (payload_length > APP_PC_LINK_MAX_PAYLOAD_BYTES))
            {
                const uint16_t sequence = app_read_le16(&g_pc_link_frame[8]);
                const app_pc_link_status_t pc_status =
                    (app_read_le32(&g_pc_link_frame[0]) != APP_PC_LINK_MAGIC) ?
                        APP_PC_LINK_STATUS_BAD_MAGIC :
                        ((g_pc_link_frame[4] != APP_PC_LINK_VERSION) ?
                             APP_PC_LINK_STATUS_UNSUPPORTED_VERSION :
                             APP_PC_LINK_STATUS_PAYLOAD_TOO_LARGE);
                product_pc_link_send_status(sequence,
                                            pc_status,
                                            APP_RESOURCE_UPDATE_STATUS_PROTOCOL);
                product_pc_link_reset_rx();
                break;
            }
            g_pc_link_expected = (uint16_t)(APP_PC_LINK_HEADER_SIZE + payload_length);
        }
        if ((g_pc_link_received >= APP_PC_LINK_HEADER_SIZE) &&
            (g_pc_link_received == g_pc_link_expected))
        {
            g_pc_link_frame_ready = true;
            product_pc_link_decode_ready_frame(now_ms);
            break;
        }
    }
}
#endif

static resource_status_t product_text_resolve(void *context,
                                              ui_language_id_t language,
                                              ui_text_id_t id,
                                              char *dst,
                                              size_t capacity)
{
    (void)context;
    if (g_resource_status != RESOURCE_STATUS_OK)
    {
        return g_resource_status;
    }
    if (!ui_text_catalog_ready(&g_text_catalog) ||
        (ui_text_catalog_language(&g_text_catalog) != (uint8_t)language))
    {
        const resource_status_t select_status =
            ui_text_catalog_select_language(&g_text_catalog, &g_resource_catalog, (uint8_t)language);
        if (select_status != RESOURCE_STATUS_OK)
        {
            if (select_status != RESOURCE_STATUS_DEFERRED)
            {
                g_resource_status = select_status;
            }
            return select_status;
        }
    }
    const resource_status_t resolve_status = ui_text_catalog_resolve(&g_text_catalog, id, dst, capacity);
    if ((resolve_status != RESOURCE_STATUS_OK) && (resolve_status != RESOURCE_STATUS_DEFERRED))
    {
        g_resource_status = resolve_status;
    }
    return resolve_status;
}

static hw_excitation_mode_t app_bsp_excitation_mode(void)
{
    switch (bsp_excitation_mode())
    {
    case BSP_EXCITATION_MODE_NEUTRAL:
        return HW_EXCITATION_MODE_NEUTRAL;
    case BSP_EXCITATION_MODE_SINE:
        return HW_EXCITATION_MODE_SINE;
    case BSP_EXCITATION_MODE_OFF:
    default:
        return HW_EXCITATION_MODE_OFF;
    }
}

static bsp_status_t product_k1_force_safe(void *user)
{
    (void)user;
    return hw_k1_force_safe(&g_k1);
}

static bsp_status_t product_k1_request_measure(const hw_safety_result_t *permission, void *user)
{
    (void)user;
    return hw_k1_request_measure(&g_k1, permission);
}

static hw_k1_state_t product_k1_commanded_state(void *user)
{
    (void)user;
    return hw_k1_commanded_state(&g_k1);
}

static bsp_status_t product_range_request(hw_range_id_t id, uint32_t now_ms, void *user)
{
    (void)user;
    return hw_range_request(&g_range, id, now_ms);
}

static bsp_status_t product_range_step(uint32_t now_ms, void *user)
{
    (void)user;
    return hw_range_step(&g_range, now_ms);
}

static bool product_range_is_ready(void *user)
{
    (void)user;
    return hw_range_is_ready(&g_range);
}

static hw_range_id_t product_range_current_id(void *user)
{
    (void)user;
    return hw_range_get_current(&g_range);
}

static hw_safety_range_state_t product_range_safety_state(void *user)
{
    (void)user;
    return hw_range_safety_state(&g_range);
}

static bsp_status_t product_range_force_disabled(void *user)
{
    (void)user;
    return hw_range_force_disabled(&g_range);
}

static void product_quiet_request(bool requested, void *user)
{
    (void)user;
    hw_peripherals_request_quiet(requested);
}

static void product_aux_pause(void *user)
{
    (void)user;
    hw_aux_sensors_pause(&g_aux_sensors);
}

static void product_aux_resume(uint32_t now_ms, void *user)
{
    (void)user;
    hw_aux_sensors_resume(&g_aux_sensors, now_ms);
}

static bsp_status_t product_adc_acquire(uint32_t now_ms, void *user)
{
    (void)user;
    return bsp_metrology_adc_acquire(now_ms);
}

static bsp_status_t product_adc_start_capture(uint32_t *raw_words,
                                              uint32_t word_count,
                                              const hw_metrology_adc_profile_t *profile,
                                              void *user)
{
    (void)user;
    if (profile == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    return bsp_metrology_adc_start_capture(raw_words, word_count, profile->tim2_arr, profile->tim2_ccr2);
}

static void product_adc_stop(void *user)
{
    (void)user;
    bsp_metrology_adc_stop();
}

static bsp_status_t product_adc_restore(uint32_t now_ms, void *user)
{
    (void)user;
    return bsp_metrology_adc_restore(now_ms);
}

static bool product_adc_dma_complete(void *user)
{
    (void)user;
    return bsp_metrology_adc_dma_complete();
}

static bool product_adc_dma_error(void *user)
{
    (void)user;
    return bsp_metrology_adc_dma_error();
}

static bsp_status_t product_excitation_off(void *user)
{
    (void)user;
    return bsp_excitation_off();
}

static bsp_status_t product_excitation_neutral(void *user)
{
    (void)user;
    return bsp_excitation_neutral();
}

static bsp_status_t product_excitation_sine(hw_excitation_freq_t frequency,
                                            hw_excitation_amp_t amplitude,
                                            void *user)
{
    (void)user;
    hw_excitation_freq_profile_t profile;
    if (hw_excitation_freq_profile(frequency, &profile) != BSP_STATUS_OK)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (hw_excitation_fill_ccr_table(g_product_ccr_table, HW_EXCITATION_LUT_POINTS, amplitude) != BSP_STATUS_OK)
    {
        return BSP_STATUS_ERROR;
    }
    return bsp_excitation_sine(profile.rcr, g_product_ccr_table, HW_EXCITATION_LUT_POINTS);
}

static hw_excitation_mode_t product_excitation_mode(void *user)
{
    (void)user;
    return app_bsp_excitation_mode();
}

static bool product_excitation_dma_error(void *user)
{
    (void)user;
    return bsp_excitation_dma_error();
}

static hw_charger_state_t product_charger_state(void *user)
{
    (void)user;
    return hw_charger_get_state(&g_charger);
}

static uint32_t product_safety_fault_mask(void *user)
{
    (void)user;
    return app_safety_fault_mask(&g_safety_faults);
}

static bsp_status_t product_permit_issue_input(hw_measure_permit_issue_input_t *input, void *user)
{
    (void)user;
    if (input == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    hw_aux_sensors_snapshot_t snapshot;
    const uint32_t now_ms = bsp_time_now_ms();
    hw_aux_sensors_snapshot(&g_aux_sensors, now_ms, &snapshot);
    input->charger = hw_charger_get_state(&g_charger);
    input->residual = snapshot.residual_state;
    input->residual_age_ms = snapshot.residual_age_ms;
    input->battery = snapshot.battery_state;
    input->battery_age_ms = snapshot.battery_age_ms;
    input->range = hw_range_safety_state(&g_range);
    input->range_id = hw_range_get_current(&g_range);
    input->k1_state = hw_k1_commanded_state(&g_k1);
    input->safety_fault_mask = app_safety_fault_mask(&g_safety_faults);
    return BSP_STATUS_OK;
}

static bsp_status_t product_permit_validate_input(hw_measure_permit_validate_input_t *input, void *user)
{
    (void)user;
    if (input == NULL)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    input->charger = hw_charger_get_state(&g_charger);
    input->range = hw_range_safety_state(&g_range);
    input->range_id = hw_range_get_current(&g_range);
    input->k1_state = hw_k1_commanded_state(&g_k1);
    input->safety_fault_mask = app_safety_fault_mask(&g_safety_faults);
    return BSP_STATUS_OK;
}

static void product_latch_k1_io_fault(void *user)
{
    (void)user;
    app_latch_fault(APP_SAFETY_FAULT_K1_IO);
}

static void product_latch_range_io_fault(void *user)
{
    (void)user;
    app_latch_fault(APP_SAFETY_FAULT_RANGE_IO);
}

static void product_latch_adc_runtime_fault(void *user)
{
    (void)user;
    app_latch_fault(APP_SAFETY_FAULT_ADC_RUNTIME);
}

static void product_latch_metrology_runtime_fault(void *user)
{
    (void)user;
    app_latch_fault(APP_SAFETY_FAULT_METROLOGY_RUNTIME);
}

static const hw_metrology_measure_io_t g_product_measure_io = {
    .k1_force_safe = product_k1_force_safe,
    .k1_request_measure = product_k1_request_measure,
    .k1_commanded_state = product_k1_commanded_state,
    .range_request = product_range_request,
    .range_step = product_range_step,
    .range_is_ready = product_range_is_ready,
    .range_current_id = product_range_current_id,
    .range_safety_state = product_range_safety_state,
    .range_force_disabled = product_range_force_disabled,
    .quiet_request = product_quiet_request,
    .aux_pause = product_aux_pause,
    .aux_resume = product_aux_resume,
    .adc_acquire = product_adc_acquire,
    .adc_start_capture = product_adc_start_capture,
    .adc_stop = product_adc_stop,
    .adc_restore = product_adc_restore,
    .adc_dma_complete = product_adc_dma_complete,
    .adc_dma_error = product_adc_dma_error,
    .excitation_off = product_excitation_off,
    .excitation_neutral = product_excitation_neutral,
    .excitation_sine = product_excitation_sine,
    .excitation_mode = product_excitation_mode,
    .excitation_dma_error = product_excitation_dma_error,
    .charger_state = product_charger_state,
    .safety_fault_mask = product_safety_fault_mask,
    .permit_issue_input = product_permit_issue_input,
    .permit_validate_input = product_permit_validate_input,
    .latch_k1_io_fault = product_latch_k1_io_fault,
    .latch_range_io_fault = product_latch_range_io_fault,
    .latch_adc_runtime_fault = product_latch_adc_runtime_fault,
    .latch_metrology_runtime_fault = product_latch_metrology_runtime_fault,
    .user = NULL,
};

static bsp_status_t product_init_metrology_measure(void)
{
    return hw_metrology_measure_init(&g_product_measure,
                                     &g_product_measure_io,
                                     app_io_workspace_metrology_raw_words(&g_io_workspace),
                                     HW_METROLOGY_RAW_WORD_COUNT);
}

static bsp_status_t product_auto_start_attempt(const hw_metrology_measure_request_t *request,
                                               uint32_t now_ms,
                                               void *user)
{
    (void)user;
    const bsp_status_t workspace_status =
        app_io_workspace_acquire(&g_io_workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
    if (workspace_status != BSP_STATUS_OK)
    {
        return workspace_status;
    }
    const bsp_status_t status = hw_metrology_measure_start(&g_product_measure, request, now_ms);
    if ((status != BSP_STATUS_OK) && (status != BSP_STATUS_BUSY))
    {
        (void)app_io_workspace_release(&g_io_workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
    }
    return status;
}

static bsp_status_t product_auto_step_attempt(uint32_t now_ms, void *user)
{
    (void)user;
    return hw_metrology_measure_step(&g_product_measure, now_ms);
}

static bool product_auto_attempt_active(void *user)
{
    (void)user;
    return hw_metrology_measure_active(&g_product_measure);
}

static bool product_auto_attempt_done(void *user)
{
    (void)user;
    return hw_metrology_measure_state(&g_product_measure) == HW_METROLOGY_MEASURE_DONE;
}

static bool product_auto_attempt_dumpable(void *user)
{
    (void)user;
    return hw_metrology_measure_dumpable(&g_product_measure);
}

static const hw_metrology_block_t *product_auto_attempt_block(void *user)
{
    (void)user;
    return hw_metrology_measure_block(&g_product_measure);
}

static hw_metrology_measure_error_t product_auto_attempt_error(void *user)
{
    (void)user;
    return hw_metrology_measure_error(&g_product_measure);
}

static void product_auto_attempt_acknowledge(void *user)
{
    (void)user;
    hw_metrology_measure_acknowledge(&g_product_measure);
    if (app_io_workspace_owner(&g_io_workspace) == APP_IO_WORKSPACE_OWNER_METROLOGY)
    {
        (void)app_io_workspace_release(&g_io_workspace, APP_IO_WORKSPACE_OWNER_METROLOGY);
    }
}

static bsp_status_t product_auto_attempt_abort(void *user)
{
    (void)user;
    return hw_metrology_measure_abort(&g_product_measure);
}

static bsp_status_t product_auto_process_block(const hw_metrology_block_t *block,
                                               const measurement_attempt_config_t *attempt,
                                               measurement_calibrated_result_t *result,
                                               void *user)
{
    (void)user;
    if ((attempt == NULL) || (result == NULL))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    const measurement_cal_key_t key = measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                                                          MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
                                                          attempt->range_id,
                                                          attempt->frequency,
                                                          attempt->amplitude);
    return measurement_cal_process_block(block,
                                         app_calibration_service_active_set(&g_calibration_service),
                                         &key,
                                         false,
                                         result);
}

static const app_measurement_session_io_t g_product_session_io = {
    .start_attempt = product_auto_start_attempt,
    .step_attempt = product_auto_step_attempt,
    .attempt_active = product_auto_attempt_active,
    .attempt_done = product_auto_attempt_done,
    .attempt_dumpable = product_auto_attempt_dumpable,
    .attempt_block = product_auto_attempt_block,
    .attempt_error = product_auto_attempt_error,
    .attempt_acknowledge = product_auto_attempt_acknowledge,
    .attempt_abort = product_auto_attempt_abort,
    .process_block = product_auto_process_block,
    .user = NULL,
};

static const app_cal_session_io_t g_product_calibration_io = {
    .start_capture = product_auto_start_attempt,
    .step_capture = product_auto_step_attempt,
    .capture_active = product_auto_attempt_active,
    .capture_done = product_auto_attempt_done,
    .capture_dumpable = product_auto_attempt_dumpable,
    .capture_block = product_auto_attempt_block,
    .capture_error = product_auto_attempt_error,
    .capture_acknowledge = product_auto_attempt_acknowledge,
    .capture_abort = product_auto_attempt_abort,
    .user = NULL,
};

static bsp_status_t product_init_controller(void)
{
    const bsp_status_t measure_status = product_init_metrology_measure();
    if (measure_status != BSP_STATUS_OK)
    {
        return measure_status;
    }
    ui_product_init(&g_product_ui);
    ui_product_set_text_provider(&g_product_ui, product_text_resolve, NULL);
    ui_product_set_font_catalog(&g_product_ui, &g_font_catalog);
#if WTK_ENABLE_PRODUCT_RICH_IMAGES
    ui_product_set_image_catalog(&g_product_ui, &g_image_catalog);
#endif
    return app_product_init(&g_product,
                            &g_calibration_service,
                            &g_settings_service,
                            &g_product_session_io,
                            &g_product_calibration_io);
}

static void product_apply_outputs(void)
{
    app_product_outputs_t outputs;
    app_product_make_outputs(&g_product, &outputs);
    if (outputs.backlight_percent != g_product_output_backlight_percent)
    {
        if (hw_backlight_set_percent(outputs.backlight_percent) == BSP_STATUS_OK)
        {
            g_product_output_backlight_percent = outputs.backlight_percent;
        }
    }
    if (!g_product_output_sound_valid ||
        (outputs.sound_enabled != g_product_output_sound_enabled))
    {
        hw_buzzer_set_enabled(outputs.sound_enabled);
        g_product_output_sound_enabled = outputs.sound_enabled;
        g_product_output_sound_valid = true;
    }
    if ((outputs.tone_sequence != 0u) &&
        (outputs.tone_sequence != g_product_output_tone_sequence))
    {
        g_product_output_tone_sequence = outputs.tone_sequence;
        if (outputs.sound_enabled)
        {
            (void)hw_buzzer_play_tone(outputs.tone_frequency_hz, outputs.tone_duration_ms, bsp_time_now_ms());
        }
    }
}
#endif

static void app_latch_fault(uint32_t fault_mask)
{
    app_safety_fault_latch(&g_safety_faults, fault_mask);
}

static void app_record_status_fault(bsp_status_t status, uint32_t fault_mask)
{
    if (status != BSP_STATUS_OK)
    {
        app_latch_fault(fault_mask);
    }
}

static uint8_t app_read_button_mask(void)
{
    uint8_t mask = 0u;
    bool active = false;

    if ((bsp_gpio_read_input(BSP_GPIO_INPUT_BUTTON_UP, &active) == BSP_STATUS_OK) && active)
    {
        mask |= (uint8_t)(1u << (uint8_t)BUTTON_ID_UP);
    }
    if ((bsp_gpio_read_input(BSP_GPIO_INPUT_BUTTON_OK, &active) == BSP_STATUS_OK) && active)
    {
        mask |= (uint8_t)(1u << (uint8_t)BUTTON_ID_OK);
    }
    if ((bsp_gpio_read_input(BSP_GPIO_INPUT_BUTTON_DOWN, &active) == BSP_STATUS_OK) && active)
    {
        mask |= (uint8_t)(1u << (uint8_t)BUTTON_ID_DOWN);
    }

    return mask;
}

#if WTK_DIAGNOSTIC_LOG_LEVEL_DEFAULT >= 4u
static void app_log_button_event(const button_event_t *event)
{
    if (event == NULL)
    {
        return;
    }

    if (bsp_quiet_requested())
    {
        return;
    }

#if WTK_ENABLE_BRINGUP_CONSOLE
    const char *button_name = "UNKNOWN";
    switch (event->button)
    {
    case BUTTON_ID_UP:
        button_name = "UP";
        break;
    case BUTTON_ID_OK:
        button_name = "OK";
        break;
    case BUTTON_ID_DOWN:
        button_name = "DOWN";
        break;
    default:
        break;
    }

    bsp_uart_write_cstr("button: ");
    bsp_uart_write_cstr(button_name);
    bsp_uart_write_cstr(" ");
    bsp_uart_write_cstr(button_event_type_string(event->type));
    bsp_uart_write_cstr("\r\n");
#else
    bsp_diagnostics_write(BSP_LOG_LEVEL_DEBUG, button_event_type_string(event->type));
#endif
}
#else
static void app_log_button_event(const button_event_t *event)
{
    (void)event;
}
#endif

#if WTK_DIAGNOSTIC_LOG_LEVEL_DEFAULT >= 4u
static const char *app_safety_state_string(app_safety_state_t state)
{
    switch (state)
    {
    case APP_SAFETY_READY:
        return "READY";
    case APP_SAFETY_WAIT_SAFE:
        return "WAIT_SAFE";
    case APP_SAFETY_SAFE_CHECK:
    default:
        return "SAFE_CHECK";
    }
}
#endif

static void app_update_safety_state(void)
{
    const uint32_t now_ms = bsp_time_now_ms();
    const hw_safety_input_t safety_input = {
        .charger = hw_charger_get_state(&g_charger),
        .residual = hw_aux_sensors_residual_state(&g_aux_sensors, now_ms),
        .battery = hw_aux_sensors_battery_state(&g_aux_sensors, now_ms),
        .range = hw_range_safety_state(&g_range),
        .application_fault = app_safety_fault_any(&g_safety_faults),
    };

    g_safety_result = hw_safety_evaluate(&safety_input);
    if (g_safety_result.measure_allowed)
    {
        g_safety_state = APP_SAFETY_READY;
    }
    else
    {
        g_safety_state = APP_SAFETY_WAIT_SAFE;
    }

    if (!hw_metrology_measure_k1_owned())
    {
        const bsp_status_t k1_safe_status = hw_k1_force_safe(&g_k1);
        app_record_status_fault(k1_safe_status, APP_SAFETY_FAULT_K1_IO);
    }

    const uint32_t range_kill_faults =
        app_safety_fault_mask(&g_safety_faults) & ~(uint32_t)APP_SAFETY_FAULT_CLOCK;
    if (range_kill_faults != 0u)
    {
        const bsp_status_t range_status = hw_range_force_disabled(&g_range);
        app_record_status_fault(range_status, APP_SAFETY_FAULT_RANGE_IO);
    }

    if (!g_safety_transition_reported ||
        (g_safety_state != g_reported_safety_state) ||
        (g_safety_result.primary_blocker != g_reported_primary_blocker))
    {
        APP_VERBOSE_DIAG_TEXT("safety_state", app_safety_state_string(g_safety_state));
        APP_VERBOSE_DIAG_TEXT("safety_block", hw_safety_primary_blocker_string(g_safety_result.primary_blocker));
        g_reported_safety_state = g_safety_state;
        g_reported_primary_blocker = g_safety_result.primary_blocker;
        g_safety_transition_reported = true;
    }
}

static void app_step(void)
{
    const uint32_t now_ms = bsp_time_now_ms();
    const bsp_status_t range_step_status = hw_range_step(&g_range, now_ms);
    if ((range_step_status != BSP_STATUS_OK) && (range_step_status != BSP_STATUS_BUSY))
    {
        app_latch_fault(APP_SAFETY_FAULT_RANGE_IO);
    }
    const bsp_status_t sensor_status = hw_aux_sensors_step(&g_aux_sensors, now_ms);
    if ((sensor_status != BSP_STATUS_OK) && (sensor_status != BSP_STATUS_BUSY))
    {
        app_latch_fault(APP_SAFETY_FAULT_ADC_RUNTIME);
    }
    if (hw_aux_sensors_fault_mask(&g_aux_sensors) != 0u)
    {
        app_latch_fault(APP_SAFETY_FAULT_ADC_RUNTIME);
    }
#if !WTK_ENABLE_BRINGUP_CONSOLE
    app_flash_access_snapshot_t flash_access = product_flash_access_snapshot(NULL);
    if (app_flash_access_allowed(&flash_access, APP_FLASH_ACCESS_CALIBRATION_MUTATION))
    {
        const bsp_status_t cal_step_status = app_calibration_service_step(&g_calibration_service, now_ms);
        if ((cal_step_status != BSP_STATUS_OK) && (cal_step_status != BSP_STATUS_BUSY))
        {
            APP_VERBOSE_DIAG_TEXT("calibration_step", bsp_status_string(cal_step_status));
        }
    }
#endif
    app_update_safety_state();

    buttons_update(&g_buttons, app_read_button_mask(), now_ms);

    button_event_t event;
    while (buttons_pop_event(&g_buttons, &event))
    {
        app_log_button_event(&event);
#if !WTK_ENABLE_BRINGUP_CONSOLE
        app_product_handle_button_event(&g_product, &event);
#endif
    }

    (void)ili9341_init_step(&g_display, now_ms);
#if WTK_ENABLE_BRINGUP_CONSOLE
    if (g_display.ready && !g_display_ready_reported)
    {
        bsp_uart_write_cstr("display: READY\r\n");
        g_display_ready_reported = true;
    }
#endif
#if WTK_ENABLE_BRINGUP_CONSOLE
    if (g_display.ready && !g_flash.detected && !g_flash_fallback_drawn)
    {
        if (ui_fallback_draw_text(&g_display, 8u, 8u, "FLASH ERROR", 0xFFFFu, 0x0000u) == BSP_STATUS_OK)
        {
            g_flash_fallback_drawn = true;
            bsp_uart_write_cstr("fallback_ui: FLASH_ERROR_DRAWN\r\n");
        }
    }
#endif

#if WTK_ENABLE_BRINGUP_CONSOLE
    app_bringup_console_step(&g_bringup_console,
                         &g_flash,
                         &g_display,
                         &g_range,
                         &g_charger,
                         &g_aux_sensors,
                         &g_k1,
                         &g_safety_result,
                         &g_safety_faults,
                         now_ms);
    if (!app_bringup_console_flash_busy(&g_bringup_console) && !app_bringup_console_capture_busy(&g_bringup_console))
    {
        (void)w25q_device_poll(&g_flash, now_ms);
    }
#else
    hw_aux_sensors_snapshot_t product_sensor_snapshot;
    hw_aux_sensors_snapshot(&g_aux_sensors, now_ms, &product_sensor_snapshot);
    const int32_t ntc_temperature_mC =
        product_sensor_snapshot.ntc_temperature_valid ?
            (int32_t)(product_sensor_snapshot.ntc_temperature_c * 1000.0f) :
            0;
    app_product_inputs_t product_inputs = {
        .calibration_status = app_calibration_service_status(&g_calibration_service),
        .calibration_active_valid = app_calibration_service_active_valid(&g_calibration_service),
        .calibration_active_sequence = app_calibration_service_active_sequence(&g_calibration_service),
        .temperature_mC = ntc_temperature_mC,
        .temperature_valid = product_sensor_snapshot.ntc_temperature_valid,
        .safety_result = g_safety_result,
        .safety_fault_mask = app_safety_fault_mask(&g_safety_faults),
        .display_fault = g_product_display_fault || (g_display.init_state == ILI9341_INIT_ERROR),
        .settings_storage_busy =
            !app_flash_access_allowed(&flash_access, APP_FLASH_ACCESS_SETTINGS_MUTATION),
        .resource_status = g_resource_status,
    };
    app_product_step(&g_product,
                     &product_inputs,
                     bsp_clock_get_summary(),
                     g_clock_status,
                     now_ms);
    ui_product_view_t product_view;
    app_product_make_view(&g_product, &product_view);
    product_apply_outputs();
    ui_product_request(&g_product_ui, &product_view);
    const bsp_status_t ui_status =
        ui_product_step(&g_product_ui, &g_display, hw_peripherals_quiet_requested());
    if ((ui_status != BSP_STATUS_OK) && (ui_status != BSP_STATUS_BUSY))
    {
        g_product_display_fault = true;
    }
#if WTK_ENABLE_PRODUCT_RESOURCE_UPDATE
    product_pc_link_step_update(now_ms);
    product_pc_link_step_rx(now_ms);
#endif
    flash_access = product_flash_access_snapshot(NULL);
    if (app_flash_access_allowed(&flash_access, APP_FLASH_ACCESS_GENERIC_POLL))
    {
        (void)w25q_device_poll(&g_flash, now_ms);
    }
#endif
    hw_buzzer_step(now_ms);
}

void app_shell_run(void)
{
    const bsp_reset_reason_t reset_reason = bsp_reset_capture_reason();

    app_safety_fault_init(&g_safety_faults);
    app_calibration_service_init(&g_calibration_service);
    app_io_workspace_init(&g_io_workspace);
    app_calibration_service_attach_workspace(&g_calibration_service, &g_io_workspace);
    const bsp_status_t gpio_status = bsp_gpio_init_safe();
    app_record_status_fault(gpio_status, APP_SAFETY_FAULT_GPIO_INIT);
    const bsp_status_t clock_status = bsp_clock_init();
    g_clock_status = clock_status;
    (void)bsp_time_init();
    (void)bsp_uart_init(115200u);

    bsp_diagnostics_boot_banner(reset_reason, clock_status);
    if (!hw_metrology_clock_ready(bsp_clock_get_summary(), clock_status))
    {
        app_latch_fault(APP_SAFETY_FAULT_CLOCK);
    }
    (void)bsp_watchdog_start();

    buttons_init(&g_buttons, NULL);
    const hw_k1_io_t k1_io = {
        .write_cmd = app_write_k1_cmd,
        .user_data = NULL,
    };
    const hw_k2_io_t k2_io = {
        .write_cmd = app_write_k2_cmd,
        .user_data = NULL,
    };
    const hw_range_io_t range_io = {
        .write_enable = app_write_range_enable,
        .write_address = app_write_range_address,
        .user_data = NULL,
    };
    const hw_charger_io_t charger_io = {
        .read_gpio = app_read_charger_gpio,
        .user_data = NULL,
    };
    const bsp_status_t k1_status = hw_k1_init(&g_k1, &k1_io);
    app_record_status_fault(k1_status, APP_SAFETY_FAULT_K1_IO);
    APP_VERBOSE_DIAG_TEXT("k1", bsp_status_string(k1_status));
    (void)bsp_excitation_init();
    const bsp_status_t k2_status = hw_k2_init(&g_k2, &k2_io);
    app_record_status_fault(k2_status, APP_SAFETY_FAULT_K2_IO);
    APP_VERBOSE_DIAG_TEXT("k2", bsp_status_string(k2_status));
    const bsp_status_t range_status = hw_range_init(&g_range, &range_io);
    app_record_status_fault(range_status, APP_SAFETY_FAULT_RANGE_IO);
    APP_VERBOSE_DIAG_TEXT("range", bsp_status_string(range_status));
    const bsp_status_t charger_init_status = hw_charger_init(&g_charger, &charger_io);
    (void)charger_init_status;
    APP_VERBOSE_DIAG_TEXT("charger_init", bsp_status_string(charger_init_status));
    const bsp_status_t adc_status = bsp_adc_init(bsp_time_now_ms());
    app_record_status_fault(adc_status, APP_SAFETY_FAULT_ADC_INIT);
    APP_VERBOSE_DIAG_TEXT("adc", bsp_status_string(adc_status));
    const hw_aux_adc_io_t aux_adc_io = {
        .start = app_adc_start,
        .poll = app_adc_poll,
        .cancel = app_adc_cancel,
        .user_data = NULL,
    };
    const bsp_status_t aux_status = hw_aux_sensors_init(&g_aux_sensors, &aux_adc_io, bsp_time_now_ms());
    app_record_status_fault(aux_status, APP_SAFETY_FAULT_ADC_INIT);
    APP_VERBOSE_DIAG_TEXT("aux_sensors", bsp_status_string(aux_status));
    APP_VERBOSE_DIAG_TEXT("charger", hw_charger_state_string(hw_charger_get_state(&g_charger)));
    APP_VERBOSE_DIAG_TEXT("k2_topology", hw_lowz_bank_mode_string(hw_k2_topology(&g_k2).lowz_bank_mode));
    APP_VERBOSE_DIAG_HEX8("safety_faults", app_safety_fault_mask(&g_safety_faults));
    g_safety_result = hw_safety_evaluate(NULL);
    g_safety_state = APP_SAFETY_SAFE_CHECK;
    g_safety_transition_reported = false;
    g_reported_safety_state = APP_SAFETY_SAFE_CHECK;
    g_reported_primary_blocker = HW_SAFETY_BLOCKED_SENSOR_INVALID;
#if WTK_ENABLE_BRINGUP_CONSOLE
    app_bringup_console_init(&g_bringup_console);
    app_bringup_console_attach_calibration_service(&g_bringup_console, &g_calibration_service);
    app_bringup_console_attach_workspace(&g_bringup_console, &g_io_workspace);
    g_display_ready_reported = false;
#else
    app_settings_service_use_defaults(&g_settings_service);
    ui_text_catalog_init(&g_text_catalog);
    ui_font_catalog_init(&g_font_catalog);
#if WTK_ENABLE_PRODUCT_RICH_IMAGES
    ui_image_catalog_init(&g_image_catalog);
#endif
    g_resource_status = RESOURCE_STATUS_MISSING;
    g_product_output_tone_sequence = 0u;
    g_product_output_backlight_percent = UINT8_MAX;
    g_product_output_sound_valid = false;
    g_product_output_sound_enabled = true;
    const bsp_status_t product_status = product_init_controller();
    app_record_status_fault(product_status, APP_SAFETY_FAULT_METROLOGY_RUNTIME);
    APP_VERBOSE_DIAG_TEXT("product", bsp_status_string(product_status));
    g_product_display_fault = false;
#endif
    w25q_device_init(&g_flash);
    ili9341_init_context(&g_display);
    g_flash_fallback_drawn = false;

    const bsp_status_t spi_status = spi_bus_init();
    (void)spi_status;
    APP_VERBOSE_DIAG_TEXT("spi2", bsp_status_string(spi_status));

    const bsp_status_t backlight_status = hw_backlight_init();
    APP_VERBOSE_DIAG_TEXT("backlight", bsp_status_string(backlight_status));
    if (backlight_status == BSP_STATUS_OK)
    {
        (void)hw_backlight_set_percent(25u);
        APP_VERBOSE_DIAG_U32("backlight_pwm_hz", hw_backlight_pwm_hz());
    }

    const bsp_status_t buzzer_status = hw_buzzer_init();
    (void)buzzer_status;
    APP_VERBOSE_DIAG_TEXT("buzzer", bsp_status_string(buzzer_status));

    const w25q_status_t flash_status = w25q_device_probe(&g_flash);
    APP_VERBOSE_DIAG_TEXT("w25q", w25q_status_string(flash_status));
    if (flash_status == W25Q_STATUS_OK)
    {
        const measurement_cal_store_io_t cal_io = measurement_cal_w25q_store_io(&g_flash);
        const bsp_status_t cal_status =
            app_calibration_service_load(&g_calibration_service, &cal_io, g_flash.part.capacity_bytes);
        (void)cal_status;
        APP_VERBOSE_DIAG_TEXT("calibration", bsp_status_string(cal_status));
        APP_VERBOSE_DIAG_TEXT("calibration_state",
                              app_calibration_service_status_string(
                                  app_calibration_service_status(&g_calibration_service)));
        const unsigned int jedec = ((unsigned int)g_flash.part.jedec.manufacturer_id << 16u) |
                                   ((unsigned int)g_flash.part.jedec.memory_type << 8u) |
                                   (unsigned int)g_flash.part.jedec.capacity_code;
        (void)jedec;
        APP_VERBOSE_DIAG_TEXT("w25q_part", g_flash.part.name);
        APP_VERBOSE_DIAG_HEX8("w25q_jedec", jedec);
        APP_VERBOSE_DIAG_U32("w25q_capacity", g_flash.part.capacity_bytes);
        APP_VERBOSE_DIAG_U32("w25q_test_sector",
                             w25q_reserved_test_sector_address(g_flash.part.capacity_bytes));
#if !WTK_ENABLE_BRINGUP_CONSOLE
        const app_settings_store_io_t settings_io = app_settings_w25q_store_io(&g_flash);
        const bsp_status_t settings_init_status =
            app_settings_service_init(&g_settings_service, &settings_io, g_flash.part.capacity_bytes);
        const bsp_status_t settings_load_status =
            (settings_init_status == BSP_STATUS_OK) ?
                app_settings_service_load(&g_settings_service, NULL) :
                settings_init_status;
        if (settings_load_status != BSP_STATUS_OK)
        {
            app_settings_service_use_defaults(&g_settings_service);
        }
        APP_VERBOSE_DIAG_TEXT("settings", bsp_status_string(settings_load_status));
        g_resource_status = product_mount_resource_pack();
        APP_VERBOSE_DIAG_TEXT("resources", resource_status_string(g_resource_status));
#endif
    }
    else
    {
        app_calibration_service_mark_storage_unavailable(&g_calibration_service);
#if !WTK_ENABLE_BRINGUP_CONSOLE
        app_settings_service_use_defaults(&g_settings_service);
#endif
        APP_VERBOSE_DIAG_TEXT("calibration_state",
                              app_calibration_service_status_string(
                                  app_calibration_service_status(&g_calibration_service)));
    }

    ili9341_init_start(&g_display, bsp_time_now_ms());
#if !WTK_ENABLE_BRINGUP_CONSOLE
    product_apply_outputs();
#endif

    for (;;)
    {
        app_step();
        bsp_diagnostics_step();
        bsp_watchdog_service();
    }
}
