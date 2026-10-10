#include "ui/ui_product.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui/ui_text_catalog.h"

static uint16_t g_frame[ILI9341_WIDTH * ILI9341_HEIGHT];
static uint8_t *g_pack;
static size_t g_pack_size;
static uint16_t g_window_x;
static uint16_t g_window_y;
static uint16_t g_window_width;
static uint16_t g_window_height;
static uint32_t g_window_cursor;
static ui_text_catalog_t g_text_en;
static ui_text_catalog_t g_text_pt;

static bsp_status_t pack_read(uint32_t address, void *dst, size_t size, void *user)
{
    (void)user;
    if ((dst == NULL) || ((uint64_t)address + size > g_pack_size))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    (void)memcpy(dst, &g_pack[address], size);
    return BSP_STATUS_OK;
}

static resource_status_t resolve_text(void *context,
                                      ui_language_id_t language,
                                      ui_text_id_t id,
                                      char *dst,
                                      size_t capacity)
{
    (void)context;
    return ui_text_catalog_resolve(language == UI_LANGUAGE_PT_BR ? &g_text_pt : &g_text_en,
                                   id, dst, capacity);
}

bsp_status_t ili9341_set_window(const ili9341_t *display,
                                uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height)
{
    (void)display;
    if ((width == 0u) || (height == 0u) ||
        ((uint32_t)x + width > ILI9341_WIDTH) ||
        ((uint32_t)y + height > ILI9341_HEIGHT))
    {
        (void)fprintf(stderr, "offscreen window: %u,%u %ux%u\n", x, y, width, height);
        return BSP_STATUS_INVALID_ARG;
    }
    g_window_x = x;
    g_window_y = y;
    g_window_width = width;
    g_window_height = height;
    g_window_cursor = 0u;
    return BSP_STATUS_OK;
}

bsp_status_t ili9341_write_pixels_rgb565(const uint16_t *pixels, size_t count)
{
    if ((pixels == NULL) || ((uint64_t)g_window_cursor + count >
                             ((uint32_t)g_window_width * g_window_height)))
    {
        return BSP_STATUS_INVALID_ARG;
    }
    for (size_t i = 0u; i < count; i++)
    {
        const uint32_t pixel = g_window_cursor++;
        const uint32_t x = (uint32_t)g_window_x + (pixel % g_window_width);
        const uint32_t y = (uint32_t)g_window_y + (pixel / g_window_width);
        g_frame[(y * ILI9341_WIDTH) + x] = pixels[i];
    }
    return BSP_STATUS_OK;
}

void ili9341_fill_start(ili9341_fill_t *fill,
                        uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t height,
                        uint16_t color_rgb565)
{
    *fill = (ili9341_fill_t){
        .x = x, .y = y, .width = width, .height = height,
        .color_rgb565 = color_rgb565,
        .remaining_pixels = (uint32_t)width * height,
        .active = true,
    };
}

bsp_status_t ili9341_fill_step(const ili9341_t *display, ili9341_fill_t *fill, uint16_t max_pixels)
{
    if ((fill == NULL) || !fill->active)
    {
        return BSP_STATUS_INVALID_ARG;
    }
    if (!fill->window_sent)
    {
        const bsp_status_t status = ili9341_set_window(display, fill->x, fill->y,
                                                        fill->width, fill->height);
        if (status != BSP_STATUS_OK)
        {
            return status;
        }
        fill->window_sent = true;
    }
    uint16_t chunk = max_pixels == 0u ? ILI9341_FILL_CHUNK_PIXELS : max_pixels;
    if ((uint32_t)chunk > fill->remaining_pixels)
    {
        chunk = (uint16_t)fill->remaining_pixels;
    }
    uint16_t pixels[ILI9341_FILL_CHUNK_PIXELS];
    for (uint16_t i = 0u; i < chunk; i++)
    {
        pixels[i] = fill->color_rgb565;
    }
    const bsp_status_t status = ili9341_write_pixels_rgb565(pixels, chunk);
    fill->remaining_pixels -= chunk;
    fill->active = fill->remaining_pixels != 0u;
    return status;
}

static bool load_pack(const char *path)
{
    FILE *file = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&file, path, "rb") != 0)
    {
        return false;
    }
#else
    file = fopen(path, "rb");
    if (file == NULL)
    {
        return false;
    }
#endif
    if (fseek(file, 0L, SEEK_END) != 0)
    {
        (void)fclose(file);
        return false;
    }
    const long size = ftell(file);
    if ((size <= 0L) || (fseek(file, 0L, SEEK_SET) != 0))
    {
        (void)fclose(file);
        return false;
    }
    g_pack_size = (size_t)size;
    g_pack = (uint8_t *)malloc(g_pack_size);
    if ((g_pack == NULL) || (fread(g_pack, 1u, g_pack_size, file) != g_pack_size))
    {
        (void)fclose(file);
        free(g_pack);
        g_pack = NULL;
        return false;
    }
    (void)fclose(file);
    return true;
}

static bool save_ppm(const char *path)
{
    FILE *file = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&file, path, "wb") != 0)
    {
        return false;
    }
#else
    file = fopen(path, "wb");
    if (file == NULL)
    {
        return false;
    }
#endif
    (void)fprintf(file, "P6\n%u %u\n255\n", ILI9341_WIDTH, ILI9341_HEIGHT);
    for (size_t i = 0u; i < (size_t)ILI9341_WIDTH * ILI9341_HEIGHT; i++)
    {
        const uint16_t color = g_frame[i];
        const uint8_t rgb[3] = {
            (uint8_t)((((color >> 11u) & 31u) * 255u) / 31u),
            (uint8_t)((((color >> 5u) & 63u) * 255u) / 63u),
            (uint8_t)(((color & 31u) * 255u) / 31u),
        };
        if (fwrite(rgb, 1u, sizeof(rgb), file) != sizeof(rgb))
        {
            (void)fclose(file);
            return false;
        }
    }
    return fclose(file) == 0;
}

static ui_product_view_t make_view(const char *scenario, ui_language_id_t language)
{
    ui_product_view_t view = {
        .state = UI_PRODUCT_STATE_READY,
        .generation = 1u,
        .menu = {.language_id = (uint8_t)language, .selected_index = 2u,
                 .brightness_percent = 75u, .timeout_seconds = 30u},
    };
    if (strcmp(scenario, "menu") == 0)
    {
        view.state = UI_PRODUCT_STATE_MENU;
    }
    else if (strcmp(scenario, "display") == 0)
    {
        view.state = UI_PRODUCT_STATE_DISPLAY_MENU;
    }
    else if (strcmp(scenario, "sound") == 0)
    {
        view.state = UI_PRODUCT_STATE_SOUND_MENU;
        view.menu.sound_enabled = true;
    }
    else if (strcmp(scenario, "language") == 0)
    {
        view.state = UI_PRODUCT_STATE_LANGUAGE_MENU;
    }
    else if (strcmp(scenario, "diagnostics") == 0)
    {
        view.state = UI_PRODUCT_STATE_DIAGNOSTICS;
    }
    else if (strcmp(scenario, "maintenance") == 0)
    {
        view.state = UI_PRODUCT_STATE_MAINTENANCE;
    }
    else if (strcmp(scenario, "calibration") == 0)
    {
        view.state = UI_PRODUCT_STATE_CALIBRATION_STATUS;
        view.calibration_status = UI_PRODUCT_CAL_ACTIVE_VALID;
        view.calibration_active_valid = true;
        view.calibration_sequence = 12u;
    }
    else if (strcmp(scenario, "wizard-intro") == 0)
    {
        view.state = UI_PRODUCT_STATE_CALIBRATION_WIZARD;
        view.wizard.state = UI_PRODUCT_WIZARD_INTRO;
    }
    else if (strcmp(scenario, "wizard-capture") == 0)
    {
        view.state = UI_PRODUCT_STATE_CALIBRATION_WIZARD;
        view.wizard = (ui_product_wizard_t){
            .state = UI_PRODUCT_WIZARD_CAPTURE_LOAD,
            .range_id = HW_RANGE_ID_1K,
            .frequency = HW_EXCITATION_FREQ_1KHZ,
            .amplitude = HW_EXCITATION_AMP_100MVRMS,
            .condition_index = 2u,
            .condition_count = 6u,
        };
    }
    else if ((strcmp(scenario, "result") == 0) ||
             (strcmp(scenario, "result-updated") == 0) ||
             (strcmp(scenario, "result-refresh") == 0))
    {
        view.state = UI_PRODUCT_STATE_RESULT;
        view.has_measurement_result = true;
        view.measurement_result = (ui_product_measurement_t){
            .status = MEASUREMENT_AUTO_STATUS_FINAL_OK,
            .interpretation = MEASUREMENT_INTERPRET_CAPACITIVE,
            .frequency = HW_EXCITATION_FREQ_1KHZ,
            .amplitude = HW_EXCITATION_AMP_100MVRMS,
            .resistance_ohms = 1.6f,
            .reactance_ohms = -3386.0f,
            .magnitude_ohms = 3386.0f,
            .phase_rad = -1.57f,
            .capacitance_f = 47.0e-9f,
            .derived_valid = true,
            .capacitance_valid = true,
        };
    }
    else if (strcmp(scenario, "details") == 0)
    {
        view = make_view("result", language);
        view.page = UI_PRODUCT_PAGE_DETAILS;
        view.measurement_result.resistance_ohms = 0.04f;
    }
    else if (strcmp(scenario, "wizard") == 0)
    {
        view.state = UI_PRODUCT_STATE_CALIBRATION_WIZARD;
        view.wizard = (ui_product_wizard_t){
            .state = UI_PRODUCT_WIZARD_WAIT_LOAD,
            .range_id = HW_RANGE_ID_1K,
            .standard = UI_PRODUCT_WIZARD_STANDARD_LOAD,
        };
    }
    else if (strcmp(scenario, "upload") == 0)
    {
        view.state = UI_PRODUCT_STATE_PC_LINK_STATUS;
    }
    else if (strcmp(scenario, "resource-error") == 0)
    {
        view.state = UI_PRODUCT_STATE_RESOURCE_ERROR;
        view.resource_status = (uint8_t)RESOURCE_STATUS_CORRUPT;
    }
    else if (strcmp(scenario, "fault") == 0)
    {
        view.state = UI_PRODUCT_STATE_SAFETY_BLOCKED;
        view.safety_blocker = UI_PRODUCT_BLOCK_RESIDUAL;
    }
    return view;
}

static bool render_until_done(ui_product_t *ui, const ili9341_t *display, uint32_t *steps)
{
    while (ui->active && *steps < 100000u)
    {
        (*steps)++;
        const bsp_status_t status = ui_product_step(ui, display, false);
        if ((status != BSP_STATUS_OK) && (status != BSP_STATUS_BUSY))
        {
            (void)fprintf(stderr, "render failed: %d\n", (int)status);
            return false;
        }
    }
    return !ui->active;
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        (void)fprintf(stderr, "usage: preview_ui_product PACK|- en|pt-BR SCENARIO OUTPUT.ppm\n");
        return 2;
    }
    const ui_language_id_t language = strcmp(argv[2], "pt-BR") == 0 ?
                                          UI_LANGUAGE_PT_BR : UI_LANGUAGE_EN;
    resource_catalog_t catalog;
    ui_font_catalog_t font;
    ui_image_catalog_t images;
    bool have_pack = strcmp(argv[1], "-") != 0;
    if (have_pack)
    {
        if (!load_pack(argv[1]))
        {
            return 3;
        }
        const resource_catalog_io_t io = {.read = pack_read};
        if ((resource_catalog_mount(&catalog, &io, 0u, (uint32_t)g_pack_size) != RESOURCE_STATUS_OK) ||
            (ui_text_catalog_select_language(&g_text_en, &catalog, UI_LANGUAGE_EN) != RESOURCE_STATUS_OK) ||
            (ui_text_catalog_select_language(&g_text_pt, &catalog, UI_LANGUAGE_PT_BR) != RESOURCE_STATUS_OK) ||
            (ui_font_catalog_mount(&font, &catalog) != RESOURCE_STATUS_OK) ||
            (ui_image_catalog_mount(&images, &catalog) != RESOURCE_STATUS_OK))
        {
            (void)fprintf(stderr, "resource mount failed\n");
            free(g_pack);
            return 4;
        }
    }
    ui_product_t ui;
    ui_product_init(&ui);
    if (have_pack)
    {
        ui_product_set_text_provider(&ui, resolve_text, NULL);
        ui_product_set_font_catalog(&ui, &font);
        ui_product_set_image_catalog(&ui, &images);
    }
    ui_product_view_t view = make_view(argv[3], language);
    /* Ephemeral host snapshot from the controller test in this same build.
       This is deliberately not a device protocol or persistent format. */
    if (argv[3][0] == '@')
    {
        FILE *snapshot = NULL;
#if defined(_MSC_VER)
        (void)fopen_s(&snapshot, argv[3] + 1, "rb");
#else
        snapshot = fopen(argv[3] + 1, "rb");
#endif
        if (snapshot == NULL) return 7;
        const bool valid = fread(&view, sizeof(view), 1u, snapshot) == 1u && fgetc(snapshot) == EOF;
        (void)fclose(snapshot);
        if (!valid) return 7;
        view.menu.language_id = (uint8_t)language;
    }
    const bool refresh = strcmp(argv[3], "result-refresh") == 0;
    if (strcmp(argv[3], "result-updated") == 0)
    {
        view.measurement_result.amplitude = HW_EXCITATION_AMP_500MVRMS;
        view.measurement_result.frequency = HW_EXCITATION_FREQ_10KHZ;
    }
    ui_product_request(&ui, &view);
    const ili9341_t display = {.ready = true};
    uint32_t steps = 0u;
    if (!render_until_done(&ui, &display, &steps))
    {
        free(g_pack);
        return 5;
    }
    if (refresh)
    {
        view.generation++;
        view.measurement_result.amplitude = HW_EXCITATION_AMP_500MVRMS;
        view.measurement_result.frequency = HW_EXCITATION_FREQ_10KHZ;
        ui_product_request(&ui, &view);
        if (!render_until_done(&ui, &display, &steps))
        {
            free(g_pack);
            return 5;
        }
    }
    const bool okay = !ui.active && save_ppm(argv[4]);
    free(g_pack);
    if (!okay)
    {
        return 6;
    }
    (void)printf("rendered %s/%s in %lu steps\n", argv[2], argv[3], (unsigned long)steps);
    return 0;
}
