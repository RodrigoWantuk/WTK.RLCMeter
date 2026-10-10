#include "app/app_cal_capture_service.h"

#include <string.h>
#include "wtk_build_config.h"

#include "app/app_version.h"
#include "measurement/measurement_condition.h"
#include "storage/storage_crc32.h"

static void copy_text(uint8_t *destination, const char *source, size_t capacity)
{
    for (size_t i = 0u; i < capacity && source[i] != '\0'; i++) destination[i] = (uint8_t)source[i];
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) | ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8u);
}

static void put32(uint8_t *p, uint32_t value)
{
    for (uint32_t i = 0u; i < 4u; i++) p[i] = (uint8_t)(value >> (i*8u));
}

static void put_float(uint8_t *p, float value)
{
    uint32_t bits;
    (void)memcpy(&bits, &value, sizeof(bits));
    put32(p, bits);
}

static float get_float(const uint8_t *p)
{
    const uint32_t bits = get32(p);
    float value;
    (void)memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool expired(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now-deadline) >= 0;
}

static app_cal_capture_snapshot_t snapshot(const app_cal_capture_service_t *service)
{
    app_cal_capture_snapshot_t result = {0};
    service->io.snapshot(&result, service->io.user);
    return result;
}

static void abort_capture(app_cal_capture_service_t *service, app_cal_capture_error_t reason)
{
    if (app_calibration_session_active(&service->session))
    {
        service->cancel_requested = true;
        service->capture_status = reason;
        (void)app_calibration_session_cancel(&service->session);
    }
    service->result_valid = false;
}

static void reset_rx(app_cal_capture_service_t *service)
{
    service->rx_size = 0u;
    service->rx_expected = APP_PC_LINK_HEADER_SIZE;
}

static void reply(app_cal_capture_service_t *service, uint8_t command, uint32_t id,
                  app_cal_capture_error_t status, const uint8_t *body, uint16_t length)
{
    if (service->tx_count == 2u)
    {
        service->protocol_errors++;
        abort_capture(service, APP_CAL_CAPTURE_BAD_FRAME);
        return;
    }
    const uint8_t slot = (uint8_t)((service->tx_head+service->tx_count)%2u);
    uint8_t *frame = service->tx[slot];
    uint8_t *payload = frame+APP_PC_LINK_HEADER_SIZE;
    payload[0] = APP_CAL_CAPTURE_API;
    payload[1] = command;
    put16(payload+2, 0u);
    put32(payload+4, id);
    put16(payload+8, (uint16_t)status);
    put16(payload+10, 0u);
    if (length != 0u) (void)memcpy(payload+12, body, length);
    app_pc_link_encode_header(frame, (app_pc_link_frame_type_t)(command|APP_CAL_CAPTURE_RESPONSE),
                             0u, (uint16_t)id, payload, (uint16_t)(12u+length));
    service->tx_size[slot] = (uint16_t)(APP_PC_LINK_HEADER_SIZE+12u+length);
    service->tx_count++;
}

static void identify(app_cal_capture_service_t *service, uint32_t id)
{
    uint8_t body[60] = {0};
    const wtk_app_version_info_t *version = wtk_app_version_get();
    put32(body, MEASUREMENT_CAL_HARDWARE_REV1);
    put16(body+4, MEASUREMENT_CAL_MODEL_VERSION_CURRENT);
    put16(body+6, 1u); /* BRINGUP_CAL service profile */
    put32(body+8, 0x0fu | (WTK_ENABLE_CAL_INSTALL_SERVICE && service->io.installation_supported ? 0x10u : 0u));
    put16(body+12, MEASUREMENT_CONDITION_REV1_MAX_SUPPORTED);
    put16(body+14, APP_CAL_CAPTURE_OBSERVATION_BYTES);
    put32(body+16, APP_CAL_CAPTURE_TIMEOUT_MS);
    put16(body+20, APP_CAL_CAPTURE_ADC_NOMINAL_3V3);
    body[22] = service->io.synthetic ? 1u : 0u;
    (void)memcpy(body+24, service->io.device_uid, 12u);
    copy_text(body+36, version->git_commit, 8u);
    copy_text(body+44, version->project_version, 15u);
    reply(service, APP_CAL_CAPTURE_IDENTIFY, id, APP_CAL_CAPTURE_OK, body, sizeof(body));
}

static void status_reply(app_cal_capture_service_t *service, uint32_t id)
{
    const app_cal_capture_snapshot_t state = snapshot(service);
    uint8_t body[32] = {0};
    put32(body, service->capture_id);
    put32(body+4, app_calibration_service_active_sequence(service->session.service));
    put32(body+8, state.safety_faults);
    put32(body+12, state.safety_blocks);
    put32(body+16, service->protocol_errors);
    put32(body+20, (uint32_t)state.temperature_mC);
    body[24] = (uint8_t)service->capture_status;
    body[25] = app_calibration_session_active(&service->session) ? 1u : 0u;
    body[26] = service->result_valid ? 1u : 0u;
    body[27] = app_calibration_service_active_valid(service->session.service) ? 1u : 0u;
    body[28] = state.transfer_safe ? 1u : 0u;
    body[29] = state.temperature_valid ? 1u : 0u;
    reply(service, APP_CAL_CAPTURE_STATUS, id, APP_CAL_CAPTURE_OK, body, sizeof(body));
}

static void start(app_cal_capture_service_t *service, uint32_t id,
                   const uint8_t *body, uint16_t length, uint32_t now_ms)
{
    app_cal_capture_error_t error = APP_CAL_CAPTURE_BAD_PAYLOAD;
    if (length != 12u)
    {
        reply(service, APP_CAL_CAPTURE_START, id, error, NULL, 0u);
        return;
    }
    app_cal_workflow_request_t request = {0};
    request.key = measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
        MEASUREMENT_CAL_MODEL_VERSION_CURRENT, (hw_range_id_t)body[0],
        (hw_excitation_freq_t)body[1], (hw_excitation_amp_t)body[2]);
    request.standard = (app_cal_standard_t){.type = (app_cal_standard_type_t)body[3],
        .z_ohms = {get_float(body+4), get_float(body+8)}, .z_valid = body[3] == APP_CAL_STANDARD_LOAD};
    const app_cal_capture_snapshot_t state = snapshot(service);
    if (app_calibration_session_active(&service->session) || app_calibration_service_busy(service->session.service) ||
        service->session.service->store_workspace_held)
        error = APP_CAL_CAPTURE_BUSY;
    else if (!measurement_condition_supported(request.key.range_id, request.key.frequency, request.key.amplitude))
        error = APP_CAL_CAPTURE_UNSUPPORTED;
    else if (body[3] > APP_CAL_STANDARD_LOAD || !measurement_complex_is_finite(request.standard.z_ohms) ||
             (body[3] == APP_CAL_STANDARD_LOAD &&
              (request.standard.z_ohms.re < 0.0f || measurement_complex_near_zero(request.standard.z_ohms, 1.0e-5f))) ||
             (body[3] != APP_CAL_STANDARD_LOAD &&
              (request.standard.z_ohms.re != 0.0f || request.standard.z_ohms.im != 0.0f)))
        error = APP_CAL_CAPTURE_BAD_PAYLOAD;
    else if (!state.factory_allowed || !state.transfer_safe)
        error = APP_CAL_CAPTURE_SAFETY_BLOCKED;
    else if (!hw_metrology_clock_ready(service->clock, service->clock_status))
        error = APP_CAL_CAPTURE_UNSUPPORTED;
    else
    {
        request.temperature_mC = state.temperature_mC;
        request.temperature_valid = state.temperature_valid;
        if (app_calibration_session_start(&service->session, &request, service->clock,
                                          service->clock_status, now_ms) == BSP_STATUS_BUSY)
        {
            service->capture_id = id;
            service->capture_status = APP_CAL_CAPTURE_BUSY;
            service->capture_deadline = now_ms+APP_CAL_CAPTURE_TIMEOUT_MS;
            service->result_valid = false;
            service->cancel_requested = false;
            service->mean_count = 0u;
            service->clip_mask = 0u;
            (void)memset(service->mean, 0, sizeof(service->mean));
            error = APP_CAL_CAPTURE_OK;
        }
        else error = APP_CAL_CAPTURE_BUSY;
    }
    reply(service, APP_CAL_CAPTURE_START, id, error, NULL, 0u);
}

static void result_reply(app_cal_capture_service_t *service, uint32_t id,
                         const uint8_t *body, uint16_t length)
{
    app_cal_capture_error_t error = APP_CAL_CAPTURE_BAD_PAYLOAD;
    uint8_t result[108] = {0};
    uint16_t written = 0u;
    if (length == 8u && get32(body) == service->capture_id && body[7] == 0u)
    {
        const uint16_t offset = get16(body+4);
        const uint16_t count = body[6];
        if (!service->result_valid) error = APP_CAL_CAPTURE_NOT_READY;
        else if (count == 0u || count > APP_CAL_CAPTURE_CHUNK_BYTES ||
                 offset >= APP_CAL_CAPTURE_OBSERVATION_BYTES || count > APP_CAL_CAPTURE_OBSERVATION_BYTES-offset)
            error = APP_CAL_CAPTURE_BAD_PAYLOAD;
        else
        {
            put32(result, service->capture_id);
            put16(result+4, offset);
            put16(result+6, APP_CAL_CAPTURE_OBSERVATION_BYTES);
            put32(result+8, get32(service->observation+184));
            (void)memcpy(result+12, service->observation+offset, count);
            written = (uint16_t)(12u+count);
            error = APP_CAL_CAPTURE_OK;
        }
    }
    reply(service, APP_CAL_CAPTURE_RESULT, id, error, result, written);
}

#if WTK_ENABLE_CAL_INSTALL_SERVICE
static bool installing(const app_cal_capture_service_t *s)
{
    return s->install_state == APP_CAL_INSTALL_RECEIVING || s->install_state == APP_CAL_INSTALL_VALIDATED ||
           s->install_state == APP_CAL_INSTALL_WRITING;
}

static uint32_t successor(const app_cal_capture_service_t *s)
{
    const uint32_t sequence = app_calibration_service_active_sequence(s->session.service);
    return sequence == UINT32_MAX ? 0u : sequence + 1u;
}

static uint32_t active_crc(const app_calibration_service_t *cal)
{
    return cal->runtime.active_valid ? cal->runtime.slots[(unsigned)cal->runtime.active_slot].frame.crc32 : 0u;
}

static void install_finish(app_cal_capture_service_t *s, app_cal_capture_error_t error)
{
    app_calibration_service_t *cal = s->session.service;
    /* A pending NOR operation must be drained by the writer before releasing its workspace. */
    (void)app_calibration_service_candidate_discard(cal);
    if (cal->store_workspace_held)
    {
        (void)app_io_workspace_release(cal->workspace, APP_IO_WORKSPACE_OWNER_CALIBRATION_STORE);
        cal->store_workspace_held = false;
    }
    s->install_error = error;
    s->install_state = error == APP_CAL_CAPTURE_CANCELED ? APP_CAL_INSTALL_ABORTED : APP_CAL_INSTALL_FAILED;
}

static bool valid_candidate(app_cal_capture_service_t *s)
{
    app_calibration_service_t *cal = s->session.service;
    const uint8_t *p = app_io_workspace_calibration_frame(cal->workspace);
    measurement_cal_frame_info_t frame;
    if (s->install_received != 2760u || storage_crc32(p, 2760u) != s->install_crc ||
        get16(p+10) != 2696u || get16(p+22) != 1u || get16(p+66) != 0u || get32(p+68) != 1u ||
        measurement_cal_inspect_frame(p, 2760u, MEASUREMENT_CAL_HARDWARE_REV1,
            MEASUREMENT_CAL_MODEL_VERSION_CURRENT, &frame).status != MEASUREMENT_CAL_VALIDITY_VALID ||
        !measurement_cal_decode_set(p, 2760u, &cal->store.scan_set, NULL) ||
        measurement_cal_validate_rev1_full_set(&cal->store.scan_set).status != MEASUREMENT_CAL_VALIDITY_VALID ||
        cal->store.scan_set.sequence != s->install_sequence || successor(s) != s->install_sequence)
        return false;
    for (size_t i = 24u; i < 56u; i++) if (p[i] != 0u) return false;
    for (size_t i = 0u; i < 6u; i++) if (get_float(p+72u+i*8u) <= 0.0f) return false;
    for (unsigned i = 0u; i < 33u; i++)
    {
        const measurement_cal_record_t *record = &cal->store.scan_set.records[i];
        const uint32_t flags = record->correction.flags;
        const uint32_t required = MEASUREMENT_CAL_FLAG_OSL_MODEL | MEASUREMENT_CAL_FLAG_LOAD_REFERENCE;
        const uint32_t allowed = required | MEASUREMENT_CAL_FLAG_TEMPERATURE_VALID | MEASUREMENT_CAL_FLAG_HG_OBSERVED;
        if ((flags & required) != required || (flags & ~allowed) != 0u ||
            record->key.hardware_revision != MEASUREMENT_CAL_HARDWARE_REV1 ||
            record->key.model_version != MEASUREMENT_CAL_MODEL_VERSION_CURRENT ||
            record->correction.load_z_ohms.re < 0.0f ||
            record->correction.reserved.re != 0.0f || record->correction.reserved.im != 0.0f ||
            record->temperature_mC < -40000 || record->temperature_mC > 125000 ||
            (!(flags & MEASUREMENT_CAL_FLAG_TEMPERATURE_VALID) && record->temperature_mC != 0)) return false;
        for (unsigned j = 70u; j < 80u; j++) if (p[120u+i*80u+j] != 0u) return false;
    }
    cal->candidate_state = APP_CAL_CANDIDATE_COMPLETE;
    return true;
}

static void install_dispatch(app_cal_capture_service_t *s, uint8_t cmd, uint32_t id,
    const uint8_t *body, uint16_t length, uint32_t now)
{
    app_calibration_service_t *cal = s->session.service;
    app_cal_capture_error_t error = APP_CAL_CAPTURE_BAD_PAYLOAD;
    uint8_t out[108] = {0};
    uint16_t count = 0u;
    const app_cal_capture_snapshot_t safe = snapshot(s);
    if (!s->io.installation_supported) error = APP_CAL_CAPTURE_UNSUPPORTED;
    else if (cmd == APP_CAL_INSTALL_STATUS && length == 0u)
    {
        put32(out, s->install_id); put32(out+4, s->install_sequence);
        put16(out+8, s->install_received); put16(out+10, 2760u);
        put32(out+12, app_calibration_service_active_sequence(cal)); put32(out+16, active_crc(cal));
        out[20] = (uint8_t)s->install_state; out[21] = (uint8_t)s->install_error;
        out[22] = (uint8_t)cal->store.state; out[23] = cal->storage_available ? 1u : 0u;
        put32(out+24, successor(s)); out[28] = (uint8_t)cal->runtime.active_slot;
        out[29] = cal->runtime.active_valid ? 1u : 0u;
        count = 32u; error = APP_CAL_CAPTURE_OK;
    }
    else if (cmd == APP_CAL_INSTALL_BEGIN && length == 12u)
    {
        if (installing(s) || app_calibration_service_busy(cal) ||
            app_io_workspace_owner(cal->workspace) != APP_IO_WORKSPACE_OWNER_FREE) error = APP_CAL_CAPTURE_BUSY;
        else if (!safe.factory_allowed || !safe.transfer_safe) error = APP_CAL_CAPTURE_SAFETY_BLOCKED;
        else if (!cal->storage_available) error = APP_CAL_CAPTURE_STORAGE_ERROR;
        else if (get32(body) != 2760u) error = APP_CAL_CAPTURE_INVALID_CANDIDATE;
        else if (successor(s) == 0u || get32(body+8) != successor(s)) error = APP_CAL_CAPTURE_SEQUENCE_ERROR;
        else if (app_calibration_service_candidate_begin(cal) != BSP_STATUS_OK) error = APP_CAL_CAPTURE_BUSY;
        else if (app_io_workspace_acquire(cal->workspace, APP_IO_WORKSPACE_OWNER_CALIBRATION_STORE) != BSP_STATUS_OK)
            error = APP_CAL_CAPTURE_BUSY;
        else
        {
            cal->store_workspace_held = true;
            s->install_id = id; s->install_sequence = get32(body+8); s->install_crc = get32(body+4);
            s->install_received = 0u; s->install_state = APP_CAL_INSTALL_RECEIVING;
            s->install_error = APP_CAL_CAPTURE_OK; s->install_deadline = now+20000u;
            s->result_valid = false; error = APP_CAL_CAPTURE_OK;
        }
    }
    else if (cmd == APP_CAL_INSTALL_READBACK && length == 8u)
    {
        const uint16_t offset = get16(body+4); const uint8_t amount = body[6];
        if (installing(s) || app_calibration_service_busy(cal)) error = APP_CAL_CAPTURE_BUSY;
        else if (!safe.transfer_safe || !safe.factory_allowed) error = APP_CAL_CAPTURE_SAFETY_BLOCKED;
        else if (!cal->runtime.active_valid || get32(body) != app_calibration_service_active_sequence(cal))
            error = APP_CAL_CAPTURE_SEQUENCE_ERROR;
        else if (body[7] != 0u || amount == 0u || amount > 96u || offset >= 2760u || amount > 2760u-offset)
            error = APP_CAL_CAPTURE_BAD_PAYLOAD;
        else
        {
            put32(out, get32(body)); put16(out+4, offset); put16(out+6, 2760u); put32(out+8, active_crc(cal));
            const uint32_t address = cal->store.slots[(unsigned)cal->runtime.active_slot].start+offset;
            error = cal->store.io.read(address,out+12,amount,cal->store.io.user) == BSP_STATUS_OK ?
                APP_CAL_CAPTURE_OK : APP_CAL_CAPTURE_STORAGE_ERROR;
            if (error == APP_CAL_CAPTURE_OK) count = (uint16_t)(12u+amount);
        }
    }
    else if (length >= 4u && get32(body) == s->install_id && s->install_id != 0u)
    {
        if (cmd == APP_CAL_INSTALL_ABORT && length == 4u)
        {
            if (s->install_state == APP_CAL_INSTALL_WRITING)
            {
                if (measurement_cal_store_abort(&cal->store) == BSP_STATUS_NOT_SUPPORTED)
                    error = APP_CAL_CAPTURE_TOO_LATE;
                else { s->install_error = APP_CAL_CAPTURE_CANCELED; error = APP_CAL_CAPTURE_OK; }
            }
            else if (installing(s)) { install_finish(s,APP_CAL_CAPTURE_CANCELED); error = APP_CAL_CAPTURE_OK; }
            else error = APP_CAL_CAPTURE_NOT_READY;
        }
        else if (!safe.factory_allowed || !safe.transfer_safe) error = APP_CAL_CAPTURE_SAFETY_BLOCKED;
        else if (cmd == APP_CAL_INSTALL_CHUNK && length > 6u && length <= 102u &&
                 s->install_state == APP_CAL_INSTALL_RECEIVING)
        {
            const uint16_t offset = get16(body+4); const uint16_t amount = (uint16_t)(length-6u);
            if (offset != s->install_received || amount > 2760u-offset) error = APP_CAL_CAPTURE_BAD_PAYLOAD;
            else
            {
                (void)memcpy(app_io_workspace_calibration_frame(cal->workspace)+offset,body+6,amount);
                s->install_received = (uint16_t)(offset+amount); s->install_deadline = now+20000u;
                error = APP_CAL_CAPTURE_OK;
            }
        }
        else if (cmd == APP_CAL_INSTALL_VALIDATE && length == 4u && s->install_state == APP_CAL_INSTALL_RECEIVING)
        {
            if (!valid_candidate(s)) { install_finish(s,APP_CAL_CAPTURE_INVALID_CANDIDATE); error = APP_CAL_CAPTURE_INVALID_CANDIDATE; }
            else { s->install_state = APP_CAL_INSTALL_VALIDATED; error = APP_CAL_CAPTURE_OK; }
        }
        else if (cmd == APP_CAL_INSTALL_COMMIT && length == 4u && s->install_state == APP_CAL_INSTALL_VALIDATED)
        {
            if (app_calibration_service_candidate_commit_bound(cal) != BSP_STATUS_BUSY)
            { install_finish(s,APP_CAL_CAPTURE_STORAGE_ERROR); error = APP_CAL_CAPTURE_STORAGE_ERROR; }
            else
            { s->install_state = APP_CAL_INSTALL_WRITING; s->install_deadline = now+30000u; error = APP_CAL_CAPTURE_OK; }
        }
        else error = APP_CAL_CAPTURE_NOT_READY;
    }
    reply(s,cmd,id,error,out,count);
}

static void install_step(app_cal_capture_service_t *s, uint32_t now)
{
    if (!installing(s)) return;
    app_calibration_service_t *cal = s->session.service;
    const app_cal_capture_snapshot_t safe = snapshot(s);
    if (expired(now,s->install_deadline) || !safe.factory_allowed || !safe.transfer_safe)
    {
        if (s->install_error == APP_CAL_CAPTURE_OK)
            s->install_error = expired(now,s->install_deadline) ? APP_CAL_CAPTURE_TIMEOUT : APP_CAL_CAPTURE_SAFETY_BLOCKED;
        if (s->install_state != APP_CAL_INSTALL_WRITING) { install_finish(s,s->install_error); return; }
        (void)measurement_cal_store_abort(&cal->store);
    }
    if (s->install_state != APP_CAL_INSTALL_WRITING) return;
    const bsp_status_t status = app_calibration_service_step(cal,now);
    if (status == BSP_STATUS_BUSY) return;
    if (status == BSP_STATUS_OK && cal->candidate_state == APP_CAL_CANDIDATE_ACTIVATED)
    { s->install_state = APP_CAL_INSTALL_INSTALLED; s->install_error = APP_CAL_CAPTURE_OK; }
    else if (status != BSP_STATUS_OK) install_finish(s,s->install_error == APP_CAL_CAPTURE_OK ?
                                                   APP_CAL_CAPTURE_STORAGE_ERROR : s->install_error);
}

#endif

static void dispatch(app_cal_capture_service_t *service, uint32_t now_ms)
{
    app_pc_link_frame_t frame;
    const app_pc_link_status_t decoded = app_pc_link_decode_frame(service->rx, service->rx_size, &frame);
    if (decoded != APP_PC_LINK_STATUS_OK)
    {
        service->protocol_errors++;
        return; /* Untrusted frame must not control hardware or supply request identity. */
    }
    if (frame.payload_length < 8u) { service->protocol_errors++; return; }
    const uint8_t command = frame.payload[1];
    const uint32_t id = get32(frame.payload+4);
    if (frame.flags != 0u || frame.payload[0] != APP_CAL_CAPTURE_API || get16(frame.payload+2) != 0u ||
        command != (uint8_t)frame.type || frame.sequence != (uint16_t)id || id == 0u)
    {
        service->protocol_errors++;
        reply(service, command, id, APP_CAL_CAPTURE_BAD_FRAME, NULL, 0u);
        return;
    }
    if (id <= service->last_request)
    {
        uint8_t last[4];
        put32(last, service->last_request);
        reply(service, command, id, APP_CAL_CAPTURE_STALE_REQUEST, last, sizeof(last));
        return;
    }
    service->last_request = id;
    const uint16_t length = (uint16_t)(frame.payload_length-8u);
    const uint8_t *body = frame.payload+8;
    if (command >= APP_CAL_INSTALL_BEGIN && command <= APP_CAL_INSTALL_ABORT)
    {
#if WTK_ENABLE_CAL_INSTALL_SERVICE
        install_dispatch(service,command,id,body,length,now_ms);
#else
        reply(service,command,id,APP_CAL_CAPTURE_UNSUPPORTED,NULL,0u);
#endif
        return;
    }
    switch (command)
    {
    case APP_CAL_CAPTURE_IDENTIFY:
        if (length == 0u) identify(service, id);
        else reply(service, command, id, APP_CAL_CAPTURE_BAD_PAYLOAD, NULL, 0u);
        break;
    case APP_CAL_CAPTURE_STATUS:
        if (length == 0u) status_reply(service, id);
        else reply(service, command, id, APP_CAL_CAPTURE_BAD_PAYLOAD, NULL, 0u);
        break;
    case APP_CAL_CAPTURE_START: start(service, id, body, length, now_ms); break;
    case APP_CAL_CAPTURE_RESULT: result_reply(service, id, body, length); break;
    case APP_CAL_CAPTURE_CANCEL:
        if (length != 4u || get32(body) != service->capture_id || service->capture_id == 0u)
            reply(service, command, id, APP_CAL_CAPTURE_BAD_PAYLOAD, NULL, 0u);
        else
        {
            abort_capture(service, APP_CAL_CAPTURE_CANCELED);
            if (!app_calibration_session_active(&service->session)) service->capture_status = APP_CAL_CAPTURE_CANCELED;
            reply(service, command, id, APP_CAL_CAPTURE_OK, NULL, 0u);
        }
        break;
    default: reply(service, command, id, APP_CAL_CAPTURE_BAD_COMMAND, NULL, 0u); break;
    }
}

bsp_status_t app_cal_capture_init(app_cal_capture_service_t *service,
    app_calibration_service_t *calibration, const app_cal_session_io_t *capture_io,
    const app_cal_capture_io_t *io, const bsp_clock_summary_t *clock, bsp_status_t clock_status)
{
    if (service == NULL || io == NULL || io->snapshot == NULL || io->try_write_byte == NULL)
        return BSP_STATUS_INVALID_ARG;
    *service = (app_cal_capture_service_t){0};
    service->io = *io;
    service->clock = clock;
    service->clock_status = clock_status;
    service->capture_status = APP_CAL_CAPTURE_NOT_READY;
    reset_rx(service);
    return app_calibration_session_init(&service->session, calibration, capture_io);
}

void app_cal_capture_receive_byte(app_cal_capture_service_t *service, uint8_t byte, uint32_t now_ms)
{
    if (service == NULL) return;
    if (service->rx_size != 0u && expired(now_ms, service->rx_deadline))
    {
        service->protocol_errors++;
        reset_rx(service);
    }
    /* Bounded magic search tolerates boot banners/partial packets without allocation. */
    static const uint8_t magic[4] = {'P', 'L', 'C', '1'};
    if (service->rx_size < 4u && byte != magic[service->rx_size])
    {
        service->rx_size = byte == magic[0] ? 1u : 0u;
        service->rx[0] = byte;
        service->rx_deadline = now_ms+APP_CAL_CAPTURE_RX_TIMEOUT_MS;
        return;
    }
    service->rx[service->rx_size++] = byte;
    service->rx_deadline = now_ms+APP_CAL_CAPTURE_RX_TIMEOUT_MS;
    if (service->rx_size == APP_PC_LINK_HEADER_SIZE)
    {
        const uint16_t size = get16(service->rx+10);
        if (service->rx[4] != APP_PC_LINK_VERSION || size > APP_PC_LINK_MAX_PAYLOAD_BYTES)
        {
            service->protocol_errors++;
            reset_rx(service);
            return;
        }
        service->rx_expected = (uint16_t)(APP_PC_LINK_HEADER_SIZE+size);
    }
    if (service->rx_size >= APP_PC_LINK_HEADER_SIZE && service->rx_size == service->rx_expected)
    {
        dispatch(service, now_ms);
        reset_rx(service);
    }
}

static measurement_complex_t scale(measurement_complex_t value, float factor)
{
    return measurement_complex(value.re * factor, value.im * factor);
}

static void accumulate(app_cal_capture_service_t *service)
{
    const app_cal_capture_sample_t *sample = &service->session.last_sample;
    const measurement_complex_t vmid = scale(
        measurement_complex_add(sample->vmid_adc1_v, sample->vmid_adc2_v), 0.5f);
    const measurement_complex_t values[6] = {
        measurement_complex_add(sample->vexc_1_v, vmid),
        measurement_complex_add(sample->ret_1x_v, vmid),
        measurement_complex_add(sample->vexc_2_v, vmid),
        measurement_complex_add(sample->ret_hg_raw_v, vmid), sample->vmid_adc1_v, sample->vmid_adc2_v};
    service->mean_count++;
    for (size_t i = 0u; i < 6u; i++)
    {
        service->mean[i] = measurement_complex_add(service->mean[i], scale(
            measurement_complex_sub(values[i], service->mean[i]), 1.0f/(float)service->mean_count));
    }
    const hw_metrology_block_t *block = service->session.io->capture_block(service->session.io->user);
    if (block != NULL)
    {
        service->permit_issue_ms = block->permit_issue_ms;
        service->permit_validate_ms = block->permit_validate_ms;
        for (uint32_t i = 0u; i < 6u; i++)
            if (block->streams[i].hard_clipped) service->clip_mask |= 1u << i;
    }
}

static void serialize_observation(app_cal_capture_service_t *service)
{
    const app_cal_evidence_t *evidence = app_calibration_session_evidence(&service->session);
    const app_cal_capture_snapshot_t state = snapshot(service);
    uint8_t *p = service->observation;
    (void)memset(p, 0, APP_CAL_CAPTURE_OBSERVATION_BYTES);
    put32(p, 0x314f4343u); /* CCO1 */
    put16(p+4, APP_CAL_CAPTURE_API);
    put16(p+6, APP_CAL_CAPTURE_OBSERVATION_BYTES);
    put32(p+8, service->capture_id);
    put32(p+12, evidence->key.hardware_revision);
    put16(p+16, evidence->key.model_version);
    p[18] = (uint8_t)evidence->key.range_id;
    p[19] = (uint8_t)evidence->key.frequency;
    p[20] = (uint8_t)evidence->key.amplitude;
    p[21] = (uint8_t)evidence->standard.type;
    uint16_t flags = 1u|2u|64u; /* stable, safe teardown, nominal ADC (NOT calibrated) */
    if (evidence->ret_1x_evidence_valid) flags |= 4u;
    if (evidence->ret_hg_evidence_valid) flags |= 8u;
    if (evidence->hg_overlap_valid && evidence->ret_1x_evidence_valid && evidence->ret_hg_evidence_valid) flags |= 16u;
    if (evidence->temperature.valid) flags |= 32u;
    if (service->clip_mask & (1u<<1u)) flags |= 128u;
    if (service->clip_mask & (1u<<3u)) flags |= 256u;
    if (service->io.synthetic) flags |= 512u;
    put16(p+22, flags);
    put32(p+24, state.safety_faults);
    put32(p+28, state.safety_blocks);
    put32(p+32, service->session.last_sample.timestamp_ms);
    put32(p+36, (uint32_t)evidence->temperature.mean_mC);
    p[40] = evidence->accepted; p[41] = evidence->rejected; p[42] = evidence->attempts;
    p[43] = (uint8_t)app_calibration_workflow_result(&service->session.service->workflow);
    put16(p+44, APP_CAL_CAPTURE_ADC_NOMINAL_3V3);
    for (size_t i = 0u; i < 6u; i++)
    {
        put_float(p+48u+i*8u, service->mean[i].re);
        put_float(p+52u+i*8u, service->mean[i].im);
    }
    const measurement_adc_calibration_t adc = measurement_adc_calibration_ideal();
    const measurement_adc_scale_t *scales[6] = {&adc.vexc_1, &adc.ret_1x, &adc.vexc_2,
        &adc.ret_hg, &adc.vmid_adc1, &adc.vmid_adc2};
    for (size_t i = 0u; i < 6u; i++)
    {
        put_float(p+96u+i*8u, scales[i]->code_to_volts);
        put_float(p+100u+i*8u, scales[i]->offset_volts);
    }
    copy_text(p+144, wtk_app_version_get()->git_commit, 8u);
    (void)memcpy(p+152, service->io.device_uid, 12u);
    put32(p+164, (uint32_t)service->session.io->capture_error(service->session.io->user));
    put32(p+168, evidence->reject_flags);
    put32(p+172, service->permit_issue_ms);
    put32(p+176, service->permit_validate_ms);
    put32(p+180, service->clip_mask);
    put32(p+184, storage_crc32(p, 184u));
    service->result_valid = true;
}

void app_cal_capture_step(app_cal_capture_service_t *service, uint32_t now_ms)
{
    if (service == NULL) return;
#if WTK_ENABLE_CAL_INSTALL_SERVICE
    install_step(service,now_ms);
#endif
    if (service->rx_size != 0u && expired(now_ms, service->rx_deadline))
    {
        service->protocol_errors++;
        reset_rx(service);
    }
    if (app_calibration_session_active(&service->session))
    {
        if (!service->cancel_requested && expired(now_ms, service->capture_deadline))
            abort_capture(service, APP_CAL_CAPTURE_TIMEOUT);
        /* START acknowledgement leaves USART before the first hardware attempt. */
        if (!(service->tx_count != 0u && service->mean_count == 0u &&
              service->session.state == APP_CAL_SESSION_START_CAPTURE && !service->cancel_requested))
        {
            const app_cal_session_event_t event = app_calibration_session_step(&service->session, now_ms);
            if (event == APP_CAL_SESSION_EVENT_CAPTURE_ACCEPTED) accumulate(service);
            if (event == APP_CAL_SESSION_EVENT_COMPLETE)
            {
                service->capture_status = APP_CAL_CAPTURE_OK;
                if (snapshot(service).transfer_safe && snapshot(service).factory_allowed &&
                    service->mean_count >= APP_CAL_WORKFLOW_REQUIRED_ACCEPTED)
                    serialize_observation(service);
                else service->capture_status = APP_CAL_CAPTURE_SAFETY_BLOCKED;
            }
            else if (event == APP_CAL_SESSION_EVENT_FAILED || event == APP_CAL_SESSION_EVENT_ERROR)
                service->capture_status = APP_CAL_CAPTURE_ACQUISITION_ERROR;
            else if (event == APP_CAL_SESSION_EVENT_CANCELED && !service->cancel_requested)
                service->capture_status = APP_CAL_CAPTURE_CANCELED;
        }
    }
    if (!snapshot(service).transfer_safe) return;
    for (uint8_t n = 0u; n < 16u && service->tx_count != 0u; n++)
    {
        if (service->io.try_write_byte(service->tx[service->tx_head][service->tx_offset], service->io.user) != BSP_STATUS_OK)
            break;
        service->tx_offset++;
        if (service->tx_offset == service->tx_size[service->tx_head])
        {
            service->tx_offset = 0u;
            service->tx_head = (uint8_t)((service->tx_head+1u)%2u);
            service->tx_count--;
        }
    }
}
