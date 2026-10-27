#include "app/app_lead_calibration.h"
#include <stdlib.h>
#undef assert
#define assert(c) do { if(!(c)) { fprintf(stderr,"assertion failed at line %u: %s\n",(unsigned)__LINE__,#c); exit(1); } } while(0)
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint8_t flash[49152];
static uint32_t raw[HW_METROLOGY_RAW_WORD_COUNT];
static hw_metrology_block_t block;
static app_calibration_service_t factory;
static app_lead_calibration_t lead;
static struct { bool active,done,draining,short_stage,weak,clip,dut; unsigned captures,releases,connections; } hw;
static bsp_status_t flash_read(uint32_t a,void *d,size_t n,void *u) { (void)u; assert(a+n<=sizeof(flash));memcpy(d,flash+a,n);return BSP_STATUS_OK; }
static bsp_status_t flash_erase(uint32_t a,uint32_t t,void *u) { (void)t;(void)u;assert(a==8192u || a==12288u);memset(flash+a,255,4096u);return BSP_STATUS_OK; }
static bsp_status_t flash_program(uint32_t a,const void *d,size_t n,uint32_t t,void *u) { (void)t;(void)u;assert(a>=8192u && a+n<=16384u);for(size_t i=0;i<n;++i)flash[a+i]&=((const uint8_t*)d)[i];return BSP_STATUS_OK; }
static bsp_status_t flash_poll(uint32_t t,void *u) { (void)t;(void)u;return BSP_STATUS_OK; }
static uint16_t code(float v) { return (uint16_t)fminf(4095.0f,fmaxf(0.0f,v*4095.0f/3.3f+0.5f)); }
static measurement_complex_t divide(measurement_complex_t a,measurement_complex_t b) { measurement_complex_t o; assert(measurement_complex_div(a,b,&o)==MEASUREMENT_STATUS_OK);return o; }
static void waveform(const hw_metrology_measure_request_t *r)
{
    static const uint32_t hz[]={10u,1000u,10000u};
    const float f=(float)hz[(unsigned)r->frequency],w=6.28318530718f*f;
    const measurement_complex_t zs={0.2f,w*1.0e-6f},yo={0.0f,w*80.0e-12f};
    const measurement_dsp_config_t config=measurement_dsp_config_ideal(r->range_id);
    const measurement_complex_t z=hw.dut?measurement_complex_add(zs,(measurement_complex_t){10.0f,0.0f}):hw.short_stage?zs:measurement_complex_add(zs,(measurement_complex_t){0.0f,-1.0f/yo.im});
    const measurement_complex_t ratio=divide(z,measurement_complex_add(z,config.zref_ohms));
    const float source=hw.weak?0.0f:0.7f;
    const measurement_complex_t vx={source*ratio.re,source*ratio.im};
    const measurement_complex_t hg=measurement_complex_mul(vx,config.ret_hg_transfer);
    hw_metrology_adc_profile_t adc; assert(hw_metrology_adc_profile(r->frequency,&adc)==BSP_STATUS_OK);
    block=(hw_metrology_block_t){.valid=true,.raw_words=raw,.dma_complete=true,.range_id=r->range_id,
        .excitation_frequency_hz=hz[(unsigned)r->frequency],.sample_count=256u,.words_per_sample=3u,
        .samples_per_cycle=adc.samples_per_cycle,.cycles_per_block=adc.cycles_per_block};
    for(unsigned i=0;i<256u;++i) {
        const float phase=6.28318530718f*(float)(i%adc.samples_per_cycle)/(float)adc.samples_per_cycle;
        const float c=cosf(phase),s=sinf(phase);
        raw[3u*i]=hw_metrology_pack_word(code(1.65f+source*c),code(1.65f+vx.re*c-vx.im*s));
        raw[3u*i+1u]=hw_metrology_pack_word(code(1.65f+source*c),code(1.65f+hg.re*c-hg.im*s));
        raw[3u*i+2u]=hw_metrology_pack_word(code(1.65f),code(1.65f));
    }
    hw_metrology_analyze_block(raw,256u,&block);
    if(hw.clip) block.streams[HW_METROLOGY_STREAM_VEXC_1].hard_clipped=true;
}
static bsp_status_t start(const hw_metrology_measure_request_t *r,uint32_t t,void *u)
{
    (void)t;(void)u;assert(hw.captures<100u);if(!hw.active)hw.connections++;
#if WTK_ENABLE_MEASUREMENT_TRIMMED
    assert(r->statistical_capture==((hw.captures%7u)!=0u));
#endif
    waveform(r); hw.captures++; hw.active=true; hw.done=false; return BSP_STATUS_OK;
}
static bsp_status_t step(uint32_t t,void *u) { (void)t;(void)u;if(hw.draining){hw.active=false;hw.draining=false;hw.releases++;}else hw.done=true;return BSP_STATUS_OK; }
static bool active(void *u) { (void)u;return hw.active; }
static bool done(void *u) { (void)u;return hw.done; }
static const hw_metrology_block_t *get_block(void *u) { (void)u;return &block; }
static hw_metrology_measure_error_t error(void *u) { (void)u;return HW_METROLOGY_MEASURE_OK; }
static void ack(void *u) { (void)u;hw.done=false; }
static bsp_status_t abort_capture(void *u) { (void)u;hw.draining=true;return BSP_STATUS_OK; }
static const app_measurement_session_io_t capture_io={.start_attempt=start,.step_attempt=step,
    .attempt_active=active,.attempt_done=done,.attempt_dumpable=done,.attempt_block=get_block,
    .attempt_error=error,.attempt_acknowledge=ack,.attempt_abort=abort_capture,
#if WTK_ENABLE_MEASURE_SESSION_HOLD
    .finish_session=abort_capture,
#endif
};
static void run_until(app_lead_state_t target)
{
    for(uint32_t t=1u;t<2000u && lead.state!=target;++t) {
        app_lead_step(&lead,NULL,BSP_STATUS_OK,true,t);
        if(lead.state==APP_LEAD_FAILED) { fprintf(stderr,"failed state at %lu samples=%u freq=%u err=%u\n",(unsigned long)t,lead.samples.summary.samples,lead.frequency,(unsigned)lead.error);break; }
    }
    assert(lead.state==target);assert(!hw.active);
}
int main(void)
{
    memset(flash,255,sizeof(flash)); app_calibration_service_init(&factory);
    factory.runtime.active_valid=true;
    measurement_cal_set_init(&factory.runtime.active_set,MEASUREMENT_CAL_HARDWARE_REV1,MEASUREMENT_CAL_MODEL_VERSION_CURRENT,11u);
    const measurement_cal_requirements_t requirements=measurement_cal_requirements_rev1_full();
    for(unsigned i=0u;i<requirements.count;++i) {
        measurement_cal_record_t r=measurement_cal_make_ideal_record(&requirements.keys[i]);
        r.correction.flags|=MEASUREMENT_CAL_FLAG_HG_OBSERVED;
        assert(measurement_cal_set_add_record(&factory.runtime.active_set,&r));
    }
    const storage_async_io_t storage={.read=flash_read,.erase_sector_start=flash_erase,.program_start=flash_program,.poll=flash_poll};
    const uint8_t uid[12]={1u};
    assert(app_lead_init(&lead,&factory,&capture_io,&storage,sizeof(flash),uid)==BSP_STATUS_OK);
    assert(app_lead_begin(&lead)==BSP_STATUS_OK);app_lead_confirm(&lead,0u);
    run_until(APP_LEAD_WAIT_SHORT);assert(hw.captures==21u && hw.releases==3u && hw.connections==3u);
    hw.short_stage=true;app_lead_confirm(&lead,0u);run_until(APP_LEAD_REVIEW);
    assert(hw.captures==42u && hw.releases==6u && hw.connections==6u && !lead.store.active_valid);
    assert(fabsf(lead.candidate.frequencies[2].series_ohms.re-0.2f)<0.005f);
    app_lead_confirm(&lead,0u);run_until(APP_LEAD_COMPLETE);
    assert(app_lead_active_profile(&lead)!=NULL && lead.store.active.sequence==1u);
    assert(app_lead_begin(&lead)==BSP_STATUS_OK);app_lead_cancel(&lead);
    assert(lead.state==APP_LEAD_CANCELED && app_lead_active_profile(&lead)!=NULL);
    assert(app_lead_set_enabled(&lead,false,false,0u)==BSP_STATUS_OK);run_until(APP_LEAD_COMPLETE);
    assert(app_lead_active_profile(&lead)==NULL && lead.store.active.sequence==2u);
    assert(app_lead_set_enabled(&lead,true,false,0u)==BSP_STATUS_OK);run_until(APP_LEAD_COMPLETE);
    assert(app_lead_active_profile(&lead)!=NULL);
    hw.dut=true;
    hw_metrology_measure_request_t dut_request={.range_id=HW_RANGE_ID_10R,.frequency=HW_EXCITATION_FREQ_10KHZ,
        .amplitude=HW_EXCITATION_AMP_100MVRMS};
    waveform(&dut_request);
    const measurement_cal_key_t dut_key=measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
        MEASUREMENT_CAL_MODEL_VERSION_CURRENT,HW_RANGE_ID_10R,HW_EXCITATION_FREQ_10KHZ,HW_EXCITATION_AMP_100MVRMS);
    measurement_calibrated_result_t corrected,uncorrected;
    assert(measurement_cal_process_block(&block,&factory.runtime.active_set,&dut_key,false,&uncorrected)==BSP_STATUS_OK);
    assert(measurement_lead_process_block(&block,&factory.runtime.active_set,&dut_key,
        app_lead_active_profile(&lead),&corrected)==BSP_STATUS_OK);
    assert(corrected.provenance.lead_compensated && corrected.provenance.lead_sequence==3u);
    assert(fabsf(corrected.result.impedance.z_ohms.re-10.0f)<0.04f);
    assert(fabsf(corrected.raw_z_ohms.re-uncorrected.result.impedance.z_ohms.re)<0.00001f);
    hw.dut=false;
    /* Actual packed ADC data rejects clipped/weak source but accepts endpoint SHORT. */
    const measurement_cal_key_t key=measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
        HW_RANGE_ID_10R,HW_EXCITATION_FREQ_10KHZ,HW_EXCITATION_AMP_100MVRMS);
    measurement_complex_t ratio;
    block.streams[HW_METROLOGY_STREAM_RET_HG].hard_clipped=true;
    assert(measurement_lead_endpoint_ratio(&block,&factory.runtime.active_set,&key,MEASUREMENT_RETURN_HG,&ratio)!=BSP_STATUS_OK);
    factory.runtime.active_set.sequence++;assert(app_lead_active_profile(&lead)==NULL);factory.runtime.active_set.sequence--;
    assert(app_lead_begin(&lead)==BSP_STATUS_OK);app_lead_confirm(&lead,0u);
    hw.captures=0u;hw.short_stage=false;
    app_lead_step(&lead,NULL,BSP_STATUS_OK,true,1u);assert(hw.active);
    app_lead_cancel(&lead);assert(lead.state==APP_LEAD_CANCELING && hw.active);
    app_lead_step(&lead,NULL,BSP_STATUS_OK,true,2u);assert(!hw.active && lead.state==APP_LEAD_CANCELED);
    assert(app_lead_active_profile(&lead)!=NULL);
    puts("lead capture: 42 packed-ADC windows, six SAFE groups, review/commit, cancel, toggle and OSL binding passed");return 0;
}
