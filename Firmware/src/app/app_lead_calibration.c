#include "app/app_lead_calibration.h"
#include <math.h>
#include <string.h>

uint32_t app_lead_factory_crc(const app_lead_calibration_t *lead)
{
    if(lead==NULL || !app_calibration_service_active_valid(lead->factory)) return 0u;
    const app_calibration_runtime_t *r=app_calibration_service_runtime_const(lead->factory);
    return r->slots[r->active_slot].frame.crc32;
}
static bool bound(const app_lead_calibration_t *lead,const measurement_lead_profile_t *p)
{
    return lead!=NULL && app_calibration_service_active_valid(lead->factory) &&
        measurement_lead_profile_bound(p,lead->uid,app_calibration_service_active_sequence(lead->factory),
                                       app_lead_factory_crc(lead));
}
const measurement_lead_profile_t *app_lead_active_profile(const app_lead_calibration_t *lead)
{
    return lead!=NULL && lead->store.loaded && lead->store.active_valid &&
        bound(lead,&lead->store.active) && lead->store.active.enabled && !lead->store.active.cleared?
        &lead->store.active:NULL;
}
bsp_status_t app_lead_init(app_lead_calibration_t *lead,app_calibration_service_t *factory,
    const app_measurement_session_io_t *io,const storage_async_io_t *flash,uint32_t capacity,const uint8_t uid[12])
{
    if(lead==NULL || factory==NULL || uid==NULL || io==NULL || io->start_attempt==NULL ||
       io->step_attempt==NULL || io->attempt_active==NULL || io->attempt_done==NULL ||
       io->attempt_dumpable==NULL || io->attempt_block==NULL || io->attempt_acknowledge==NULL ||
       io->attempt_abort==NULL) return BSP_STATUS_INVALID_ARG;
    *lead=(app_lead_calibration_t){.factory=factory,.io=io}; memcpy(lead->uid,uid,12u);
    const bsp_status_t initialized=lead_profile_store_init(&lead->store,flash,capacity);
    if(initialized!=BSP_STATUS_OK) return initialized;
    lead->error=lead_profile_store_load(&lead->store); return lead->error;
}
bool app_lead_busy(const app_lead_calibration_t *lead)
{
    return lead!=NULL && ((lead->state>=APP_LEAD_WAIT_OPEN && lead->state<=APP_LEAD_SAVING) ||
                         lead->state==APP_LEAD_CANCELING);
}
bsp_status_t app_lead_begin(app_lead_calibration_t *lead)
{
    if(lead==NULL || !lead->store.initialized || !lead->store.loaded || app_lead_busy(lead) ||
       !app_calibration_service_active_valid(lead->factory) || lead->io->attempt_active(lead->io->user))
        return BSP_STATUS_ERROR;
    lead->candidate=(measurement_lead_profile_t){.factory_sequence=app_calibration_service_active_sequence(lead->factory),
        .factory_crc32=app_lead_factory_crc(lead),.hardware_revision=MEASUREMENT_CAL_HARDWARE_REV1,
        .model_version=MEASUREMENT_CAL_MODEL_VERSION_CURRENT,.coverage=7u,.enabled=true};
    memcpy(lead->candidate.uid,lead->uid,12u); lead->short_stage=false; lead->frequency=0u;
    lead->open_range=(uint8_t)HW_RANGE_ID_1M; lead->error=BSP_STATUS_OK;
    lead->cancel_requested=false; lead->finish_failed=false; lead->state=APP_LEAD_WAIT_OPEN; return BSP_STATUS_OK;
}
static void begin_group(app_lead_calibration_t *lead,uint32_t now_ms)
{
    measurement_trimmed_init(&lead->samples); lead->attempts=0u;
    lead->started_ms=now_ms; lead->range_retry=false; lead->state=APP_LEAD_START;
}
void app_lead_confirm(app_lead_calibration_t *lead,uint32_t now_ms)
{
    if(lead==NULL) return;
    if(lead->state==APP_LEAD_WAIT_OPEN || lead->state==APP_LEAD_WAIT_SHORT) begin_group(lead,now_ms);
    else if(lead->state==APP_LEAD_REVIEW) {
        lead->error=lead_profile_store_save(&lead->store,&lead->candidate,now_ms);
        lead->state=lead->error==BSP_STATUS_OK?APP_LEAD_SAVING:APP_LEAD_FAILED;
    }
}
void app_lead_cancel(app_lead_calibration_t *lead)
{
    if(lead==NULL || !app_lead_busy(lead) || lead->state==APP_LEAD_SAVING) return;
    lead->cancel_requested=true;
    if(lead->io->attempt_active(lead->io->user)) {
        (void)lead->io->attempt_abort(lead->io->user); lead->state=APP_LEAD_CANCELING;
    } else lead->state=APP_LEAD_CANCELED;
}
static measurement_cal_key_t current_key(const app_lead_calibration_t *lead)
{
    return measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
        lead->short_stage?HW_RANGE_ID_10R:(hw_range_id_t)lead->open_range,
        (hw_excitation_freq_t)lead->frequency,
        lead->short_stage?HW_EXCITATION_AMP_100MVRMS:HW_EXCITATION_AMP_500MVRMS);
}
static bool coefficients(app_lead_calibration_t *lead,const measurement_cal_key_t *key,
                         measurement_cal_osl_coefficients_t *out)
{
    measurement_cal_resolved_t r;
    const measurement_cal_resolve_status_t status=measurement_cal_resolve_condition(
        app_calibration_service_active_set(lead->factory),key,false,&r);
    return (status==MEASUREMENT_CAL_RESOLVE_FOUND || status==MEASUREMENT_CAL_RESOLVE_UNQUALIFIED) &&
        measurement_cal_get_osl_coefficients(&r.correction,out);
}
static void stop_error(app_lead_calibration_t *lead,bsp_status_t error)
{
    lead->error=error; lead->finish_failed=true;
    if(lead->io->attempt_active(lead->io->user)) {
        (void)lead->io->attempt_abort(lead->io->user); lead->state=APP_LEAD_CANCELING;
    } else lead->state=APP_LEAD_FAILED;
}
static bool complete_group(app_lead_calibration_t *lead)
{
    if(!measurement_trimmed_finish(&lead->samples)) return false;
    /* Engineering repeatability gate in the dimensionless transfer plane.
     * Require all accepted windows near their trimmed estimate; no false metrology claim. */
    const float limit=0.001f+0.01f*measurement_complex_mag(lead->samples.estimate);
    for(unsigned i=0u;i<7u;++i)
        if(measurement_complex_mag(measurement_complex_sub(lead->samples.points[i],lead->samples.estimate))>limit)
            return false;
    const unsigned f=lead->frequency; const measurement_cal_key_t key=current_key(lead);
    if(!lead->short_stage) { lead->open_ratio[f]=lead->samples.estimate; lead->open_key[f]=key; return true; }
    measurement_cal_osl_coefficients_t c;
    measurement_lead_frequency_t *out=&lead->candidate.frequencies[f];
    if(!coefficients(lead,&key,&c) || measurement_lead_series(&c,lead->samples.estimate,&out->series_ohms)!=MEASUREMENT_STATUS_OK ||
       !coefficients(lead,&lead->open_key[f],&c) ||
       measurement_lead_shunt(&c,lead->open_ratio[f],out->series_ohms,&out->shunt_siemens)!=MEASUREMENT_STATUS_OK)
        return false;
    static const uint32_t hz[]={10u,1000u,10000u};
    return measurement_lead_frequency_valid(out,hz[f]);
}
void app_lead_step(app_lead_calibration_t *lead,const bsp_clock_summary_t *clock,
    bsp_status_t clock_status,bool storage_allowed,uint32_t now_ms)
{
    if(lead==NULL || !app_lead_busy(lead)) return;
    if(lead->state==APP_LEAD_SAVING) {
        if(storage_allowed) {
            const bsp_status_t status=lead_profile_store_step(&lead->store,now_ms);
            if(status!=BSP_STATUS_BUSY) { lead->error=status; lead->state=status==BSP_STATUS_OK?APP_LEAD_COMPLETE:APP_LEAD_FAILED; }
        }
        return;
    }
    if(lead->state==APP_LEAD_CANCELING) {
        (void)lead->io->step_attempt(now_ms,lead->io->user);
        /* A result-ready hold remains physically connected: only !active is SAFE. */
        if(!lead->io->attempt_active(lead->io->user)) {
            lead->io->attempt_acknowledge(lead->io->user);
            lead->state=lead->finish_failed?APP_LEAD_FAILED:APP_LEAD_CANCELED;
        }
        return;
    }
    if(!bound(lead,&lead->candidate)) { stop_error(lead,BSP_STATUS_ERROR); return; }
    if(lead->state==APP_LEAD_WAIT_OPEN || lead->state==APP_LEAD_WAIT_SHORT || lead->state==APP_LEAD_REVIEW) return;
    if((uint32_t)(now_ms-lead->started_ms)>=25000u) { stop_error(lead,BSP_STATUS_TIMEOUT); return; }
    switch(lead->state) {
    case APP_LEAD_START: {
        if(lead->attempts>=9u) {
            if(!lead->short_stage && lead->open_range>(uint8_t)HW_RANGE_ID_10K) {
                lead->range_retry=true; lead->state=APP_LEAD_DRAIN;
            } else stop_error(lead,BSP_STATUS_ERROR);
            break;
        }
        const measurement_cal_key_t key=current_key(lead);
        hw_metrology_measure_request_t request={.clock_summary=clock,.clock_init_status=clock_status,
            .range_id=key.range_id,.frequency=key.frequency,.amplitude=key.amplitude};
#if WTK_ENABLE_MEASURE_SESSION_HOLD
        request.session_hold=true;
#if WTK_ENABLE_CONTINUOUS_SURVEY
        request.continuous_survey=true;
#endif
#if WTK_ENABLE_MEASUREMENT_TRIMMED
        request.statistical_capture=lead->attempts!=0u;
#endif
#endif
        const bsp_status_t status=lead->io->start_attempt(&request,now_ms,lead->io->user);
        if(status!=BSP_STATUS_OK && status!=BSP_STATUS_BUSY) { stop_error(lead,status); break; }
        lead->attempts++; lead->state=APP_LEAD_WAIT; break;
    }
    case APP_LEAD_WAIT: {
        const bsp_status_t status=lead->io->step_attempt(now_ms,lead->io->user);
        if(!lead->io->attempt_done(lead->io->user)) {
            if(status!=BSP_STATUS_OK && status!=BSP_STATUS_BUSY) stop_error(lead,status);
            break;
        }
        if(!lead->io->attempt_dumpable(lead->io->user)) { stop_error(lead,BSP_STATUS_ERROR); break; }
        const measurement_cal_key_t key=current_key(lead); measurement_complex_t ratio;
        const bsp_status_t captured=measurement_lead_endpoint_ratio(lead->io->attempt_block(lead->io->user),
            app_calibration_service_active_set(lead->factory),&key,
            lead->short_stage?MEASUREMENT_RETURN_HG:MEASUREMENT_RETURN_1X,&ratio);
        lead->io->attempt_acknowledge(lead->io->user);
        if(captured==BSP_STATUS_OK) (void)measurement_trimmed_add(&lead->samples,ratio);
        if(lead->samples.summary.samples==7u) {
            if(!complete_group(lead)) {
                if(!lead->short_stage && lead->open_range>(uint8_t)HW_RANGE_ID_10K) lead->range_retry=true;
                else { stop_error(lead,BSP_STATUS_ERROR); break; }
            }
            lead->state=APP_LEAD_DRAIN;
        } else lead->state=APP_LEAD_START;
        break;
    }
    case APP_LEAD_DRAIN:
#if WTK_ENABLE_MEASURE_SESSION_HOLD
        if(lead->io->finish_session!=NULL) (void)lead->io->finish_session(lead->io->user);
#endif
        (void)lead->io->step_attempt(now_ms,lead->io->user);
        if(!lead->io->attempt_active(lead->io->user)) {
            if(lead->io->attempt_error!=NULL && lead->io->attempt_error(lead->io->user)!=HW_METROLOGY_MEASURE_OK) {
                stop_error(lead,BSP_STATUS_ERROR); break;
            }
            lead->io->attempt_acknowledge(lead->io->user);
            if(lead->range_retry) { lead->open_range--; begin_group(lead,now_ms); break; }
            lead->frequency++;
            if(lead->frequency<3u) {
                if(!lead->short_stage) lead->open_range=(uint8_t)HW_RANGE_ID_1M;
                begin_group(lead,now_ms);
            }
            else if(!lead->short_stage) {
                lead->short_stage=true; lead->frequency=0u; lead->state=APP_LEAD_WAIT_SHORT;
            } else lead->state=measurement_lead_profile_valid(&lead->candidate)?APP_LEAD_REVIEW:APP_LEAD_FAILED;
        }
        break;
    default: break;
    }
}
bsp_status_t app_lead_set_enabled(app_lead_calibration_t *lead,bool enabled,bool clear,uint32_t now_ms)
{
    if(lead==NULL || app_lead_busy(lead) || !lead->store.active_valid || !bound(lead,&lead->store.active) ||
       (enabled && lead->store.active.cleared)) return BSP_STATUS_ERROR;
    measurement_lead_profile_t candidate=lead->store.active;
    candidate.enabled=enabled && !clear; candidate.cleared=clear || candidate.cleared;
    const bsp_status_t status=lead_profile_store_save(&lead->store,&candidate,now_ms);
    lead->error=status; lead->state=status==BSP_STATUS_OK?APP_LEAD_SAVING:APP_LEAD_FAILED; return status;
}
