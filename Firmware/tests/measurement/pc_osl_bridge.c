/* Host-only oracle for PC OSL compatibility and the existing C A/B store FSM. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_pc_link_protocol.h"
#include "measurement/measurement_calibration_solver.h"
#include "measurement/measurement_calibration_store.h"

enum { CAPACITY = 65536u, SLOT_IMAGE_BYTES = 8192u, PAGE_BYTES = 256u };
static uint8_t input[MEASUREMENT_CAL_MAX_FRAME_BYTES];
static uint8_t image[MEASUREMENT_CAL_MAX_FRAME_BYTES];
static uint8_t flash[CAPACITY];
static uint8_t pending[PAGE_BYTES];
static uint32_t pending_address;
static size_t pending_size;
static bool erase_pending;
static measurement_cal_set_t set;
static measurement_cal_store_t store;

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

static float f32(const uint8_t *p)
{
    const uint32_t bits = u32(p);
    float result;
    (void)memcpy(&result, &bits, sizeof(result));
    return result;
}

static measurement_complex_t complex_at(const uint8_t *p)
{
    return measurement_complex(f32(p), f32(p+4));
}

static bool read_file(const char *path, uint8_t *dst, size_t capacity, size_t *size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    *size = fread(dst, 1u, capacity, file);
    const bool ok = !ferror(file) && (fgetc(file) == EOF);
    (void)fclose(file);
    return ok;
}

static bool write_file(const char *path, const uint8_t *src, size_t size)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) return false;
    const bool ok = fwrite(src, 1u, size, file) == size;
    return fclose(file) == 0 && ok;
}

static bool solve_input(size_t size)
{
    if (size != 56u + 33u*84u || memcmp(input, "PCOS", 4u) != 0) return false;
    measurement_cal_set_init(&set, MEASUREMENT_CAL_HARDWARE_REV1,
                             MEASUREMENT_CAL_MODEL_VERSION_CURRENT, u32(input+4));
    if (set.sequence == 0u) return false;
    set.adc_valid = true;
    measurement_adc_scale_t *scales[] = {&set.adc.vexc_1, &set.adc.ret_1x,
        &set.adc.vexc_2, &set.adc.ret_hg, &set.adc.vmid_adc1, &set.adc.vmid_adc2};
    for (size_t i = 0u; i < 6u; i++)
    {
        scales[i]->code_to_volts = f32(input+8u+i*8u);
        scales[i]->offset_volts = f32(input+12u+i*8u);
    }
    for (size_t i = 0u; i < 33u; i++)
    {
        const uint8_t *row = input+56u+i*84u;
        const measurement_cal_key_t key = measurement_cal_key(
            MEASUREMENT_CAL_HARDWARE_REV1, MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
            (hw_range_id_t)row[0], (hw_excitation_freq_t)row[1], (hw_excitation_amp_t)row[2]);
        measurement_cal_solver_input_t triplet = {0};
        measurement_cal_solver_standard_t *standards[] = {&triplet.open, &triplet.shorted, &triplet.load};
        for (size_t j = 0u; j < 3u; j++)
        {
            const uint8_t *s = row+12u+j*24u;
            const uint32_t flags = u32(s+20);
            int32_t temperature;
            const uint32_t bits = u32(s+16);
            (void)memcpy(&temperature, &bits, sizeof(temperature));
            *standards[j] = (measurement_cal_solver_standard_t){
                .key = key, .standard = (measurement_cal_standard_type_t)j,
                .present = true, .stable = true, .standard_z_valid = j == 2u,
                .standard_z_ohms = j == 2u ? complex_at(row+4) : measurement_complex(0.0f, 0.0f),
                .t_1x = complex_at(s), .t_hg_raw = complex_at(s+8),
                .ret_1x_valid = (flags & 1u) != 0u, .ret_hg_valid = (flags & 2u) != 0u,
                .hg_observed_valid = (flags & 4u) != 0u, .temperature_valid = (flags & 8u) != 0u,
                .temperature_mC = temperature,
            };
        }
        measurement_cal_solver_solution_t solution;
        const measurement_cal_solver_status_t status = measurement_cal_solver_solve(&triplet, &solution);
        if (status != MEASUREMENT_CAL_SOLVER_OK)
        {
            (void)fprintf(stderr, "solver[%lu]=%s\n", (unsigned long)i,
                          measurement_cal_solver_status_string(status));
            return false;
        }
        const measurement_cal_record_t record = measurement_cal_solver_make_record(&solution);
        if (!measurement_cal_set_add_record(&set, &record)) return false;
    }
    return measurement_cal_validate_rev1_full_set(&set).status == MEASUREMENT_CAL_VALIDITY_VALID;
}

static bsp_status_t fake_read(uint32_t address, void *dst, size_t size, void *user)
{
    (void)user;
    if (address > CAPACITY || size > CAPACITY-address) return BSP_STATUS_INVALID_ARG;
    (void)memcpy(dst, flash+address, size);
    return BSP_STATUS_OK;
}

static bsp_status_t fake_erase(uint32_t address, uint32_t now, void *user)
{
    (void)now; (void)user;
    if (address % 4096u != 0u || address > CAPACITY-4096u) return BSP_STATUS_INVALID_ARG;
    pending_address = address; erase_pending = true;
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_program(uint32_t address, const void *src, size_t size, uint32_t now, void *user)
{
    (void)now; (void)user;
    if (size == 0u || size > PAGE_BYTES || address > CAPACITY || size > CAPACITY-address ||
        address/PAGE_BYTES != (address+(uint32_t)size-1u)/PAGE_BYTES) return BSP_STATUS_INVALID_ARG;
    pending_address = address; pending_size = size;
    (void)memcpy(pending, src, size);
    return BSP_STATUS_BUSY;
}

static bsp_status_t fake_poll(uint32_t now, void *user)
{
    (void)now; (void)user;
    if (erase_pending)
    {
        (void)memset(flash+pending_address, 0xff, 4096u); erase_pending = false;
    }
    else
    {
        for (size_t i = 0u; i < pending_size; i++) flash[pending_address+i] &= pending[i];
        pending_size = 0u;
    }
    return BSP_STATUS_OK;
}

static int store_candidate(const char *old_slots, const char *new_slots, uint32_t cut)
{
    const size_t start = CAPACITY-STORAGE_LAYOUT_MUTABLE_RESERVED_BYTES;
    size_t size;
    (void)memset(flash, 0xff, sizeof(flash));
    if (!read_file(old_slots, flash+start, SLOT_IMAGE_BYTES, &size) || size != SLOT_IMAGE_BYTES) return 2;
    const measurement_cal_store_io_t io = {fake_read, fake_erase, fake_program, fake_poll, NULL};
    const measurement_cal_requirements_t requirements = measurement_cal_requirements_rev1_full();
    if (measurement_cal_store_init(&store, &io, CAPACITY, image, sizeof(image)) != BSP_STATUS_OK ||
        measurement_cal_store_write_start(&store, &set, &requirements) != BSP_STATUS_BUSY) return 2;
    uint32_t steps = 0u;
    while (store.state != MEASUREMENT_CAL_STORE_DONE && store.state != MEASUREMENT_CAL_STORE_ERROR && steps < cut)
    {
        (void)measurement_cal_store_step(&store, steps++);
    }
    const measurement_cal_store_state_t state = store.state;
    if (!write_file(new_slots, flash+start, SLOT_IMAGE_BYTES)) return 2;
    /* Reboot: lose volatile pending operations and reconstruct from persisted bytes. */
    pending_size = 0u; erase_pending = false;
    if (measurement_cal_store_init(&store, &io, CAPACITY, image, sizeof(image)) != BSP_STATUS_OK) return 2;
    const bsp_status_t loaded = measurement_cal_store_load_newest_usable(
        &store, &requirements, MEASUREMENT_CAL_HARDWARE_REV1, MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
        &set, NULL, NULL);
    (void)printf("state=%u steps=%lu active_sequence=%lu\n", (unsigned int)state,
                 (unsigned long)steps, (unsigned long)(loaded == BSP_STATUS_OK ? set.sequence : 0u));
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    size_t size;
    if (!read_file(argv[2], input, sizeof(input), &size)) return 2;
    if (strcmp(argv[1], "wire") == 0)
    {
        app_pc_link_frame_t frame;
        if (app_pc_link_decode_frame(input, size, &frame) != APP_PC_LINK_STATUS_OK) return 2;
        app_pc_link_encode_header(image, frame.type, frame.flags, frame.sequence,
                                  frame.payload, frame.payload_length);
        (void)memcpy(image+APP_PC_LINK_HEADER_SIZE, frame.payload, frame.payload_length);
        return write_file(argv[3], image, APP_PC_LINK_HEADER_SIZE+frame.payload_length) ? 0 : 2;
    }
    if (strcmp(argv[1], "solve") == 0)
    {
        if (!solve_input(size)) return 2;
    }
    else if (!measurement_cal_decode_set(input, size, &set, NULL) ||
             measurement_cal_validate_rev1_full_set(&set).status != MEASUREMENT_CAL_VALIDITY_VALID) return 2;
    if (strcmp(argv[1], "store") == 0)
    {
        if (argc != 6) return 2;
        return store_candidate(argv[3], argv[4], (uint32_t)strtoul(argv[5], NULL, 10));
    }
    size_t written;
    if (!measurement_cal_serialize_set(&set, image, sizeof(image), &written)) return 2;
    return write_file(argv[3], image, written) ? 0 : 2;
}
