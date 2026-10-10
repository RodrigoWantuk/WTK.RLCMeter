/* Synthetic ADC fixture; production PLC1 dispatch, session and DSP are linked unchanged. */
#include "app/app_cal_capture_service.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static app_cal_capture_service_t capture;
static app_calibration_service_t calibration;
static app_io_workspace_t workspace;
static hw_metrology_block_t block;
static uint8_t output[4096];
static size_t output_size;
static uint32_t now, injection;
static bool active, done, aborted;
static unsigned ticks;
static const bsp_clock_summary_t clock_info = {.source=BSP_CLOCK_SOURCE_HSE_PLL,
    .hse_ready=true,.sysclk_hz=72000000u,.hclk_hz=72000000u,.pclk1_hz=36000000u,
    .pclk2_hz=72000000u,.tim_apb1_hz=72000000u,.tim_apb2_hz=72000000u,.adc_hz=12000000u};

enum { CAPACITY=2097152u, BASE=CAPACITY-STORAGE_LAYOUT_MUTABLE_RESERVED_BYTES };
static uint8_t nor[8192], pending[256];
static uint32_t pending_address, operations;
static uint32_t pending_started;
static size_t pending_size;
static unsigned pending_kind, poll_ticks;
static bsp_status_t flash_read(uint32_t address,void *dst,size_t size,void *user)
{
    (void)user;
    if(address<BASE || address-BASE>=sizeof(nor) || size>sizeof(nor)-(address-BASE)) return BSP_STATUS_ERROR;
    memcpy(dst,nor+(address-BASE),size);
    if((injection&32u) && size!=0u) ((uint8_t *)dst)[0]^=1u;
    return BSP_STATUS_OK;
}
static bsp_status_t flash_erase(uint32_t address,uint32_t time,void *user)
{
    (void)time;(void)user;
    if(pending_kind || address<BASE || address-BASE>=sizeof(nor) || (address%4096u)!=0u) return BSP_STATUS_ERROR;
    pending_kind=1u;pending_address=address-BASE;pending_size=4096u;poll_ticks=0u;operations++;pending_started=time;
    return BSP_STATUS_BUSY;
}
static bsp_status_t flash_program(uint32_t address,const void *src,size_t size,uint32_t time,void *user)
{
    (void)time;(void)user;
    if(pending_kind || address<BASE || address-BASE>=sizeof(nor) || size>sizeof(pending) ||
        size>256u-address%256u || size>sizeof(nor)-(address-BASE)) return BSP_STATUS_ERROR;
    memcpy(pending,src,size);pending_kind=2u;pending_address=address-BASE;pending_size=size;
    poll_ticks=0u;operations++;pending_started=time;return BSP_STATUS_BUSY;
}
static void apply_pending(size_t amount)
{
    if(amount>pending_size) amount=pending_size;
    for(size_t i=0;i<amount;i++)
        if(pending_kind==1u) nor[pending_address+i]=0xffu;
        else if(pending_kind==2u) nor[pending_address+i]&=pending[i];
    pending_kind=0u;
}
static bsp_status_t flash_poll(uint32_t time,void *user)
{
    (void)time;(void)user;
    if(injection&128u)
    {
        if(time-pending_started<1000u) return BSP_STATUS_BUSY;
        pending_kind=0u;return BSP_STATUS_TIMEOUT;
    }
    if(++poll_ticks<2u) return BSP_STATUS_BUSY;
    const bool fail=(injection&64u)!=0u;
    apply_pending(fail?pending_size/2u:pending_size);
    return fail?BSP_STATUS_ERROR:BSP_STATUS_OK;
}
static const measurement_cal_store_io_t flash_io={.read=flash_read,.erase_sector_start=flash_erase,
    .program_start=flash_program,.poll=flash_poll};

static uint16_t raw(float volts)
{
    int value=(int)(volts*(4095.0f/3.3f)+0.5f);
    if(value<0) value=0;
    if(value>4095) value=4095;
    return (uint16_t)value;
}
static bsp_status_t start_capture(const hw_metrology_measure_request_t *request,uint32_t time,void *user)
{
    (void)user;
    if(injection&3u) return BSP_STATUS_ERROR;
    uint32_t *words=app_io_workspace_metrology_raw_words(&workspace);
    hw_metrology_adc_profile_t profile;
    hw_excitation_freq_profile_t freq;
    (void)hw_metrology_adc_profile(request->frequency,&profile);
    (void)hw_excitation_freq_profile(request->frequency,&freq);
    memset(&block,0,sizeof(block));
    block.valid=true; block.mode=HW_METROLOGY_MODE_DUT_MEASURE; block.dut_measure=true;
    block.excitation_frequency_hz=freq.frequency_hz;
    block.requested_amplitude_mvrms=hw_excitation_amplitude_mvrms(request->amplitude);
    block.range_id=request->range_id; block.adc_clock_hz=12000000u;
    block.sample_time_cycles_x2=HW_METROLOGY_ADC_SAMPLE_TIME_CYCLES_X2;
    block.sample_rate_hz=profile.sample_rate_hz; block.samples_per_cycle=profile.samples_per_cycle;
    block.cycles_per_block=profile.cycles_per_block; block.sample_count=HW_METROLOGY_SAMPLES_PER_BLOCK;
    block.words_per_sample=HW_METROLOGY_WORDS_PER_SAMPLE; block.raw_words=words; block.dma_complete=true;
    block.permit_issue_ms=time; block.permit_validate_ms=time;
    /* Electrical fixture: series Z/Rref + shunt Y*Rref, then voltage-divider transfer.
       No calibration coefficients are used to synthesize these ADC samples. */
    const app_cal_workflow_request_t *wf=&calibration.workflow.request;
    const float ranges[6]={10.0f,100.0f,1000.0f,10000.0f,100000.0f,1000000.0f};
    measurement_complex_t y=measurement_complex(0.0003f,0.00008f),t;
    if(wf->standard.type!=APP_CAL_STANDARD_OPEN)
    {
        measurement_complex_t z=measurement_complex(0.08f,0.002f);
        if(wf->standard.type==APP_CAL_STANDARD_LOAD)
            z=measurement_complex_add(z,measurement_complex_mul(wf->standard.z_ohms,
                measurement_complex(1.0f/ranges[(unsigned)request->range_id],0.0f)));
        measurement_complex_t inv;
        (void)measurement_complex_div(measurement_complex(1.0f,0.0f),z,&inv);
        y=measurement_complex_add(y,inv);
    }
    (void)measurement_complex_div(measurement_complex(1.0f,0.0f),
        measurement_complex_add(measurement_complex(1.0f,0.0f),y),&t);
    const float amplitude=(float)block.requested_amplitude_mvrms*0.001414213562f;
    measurement_complex_t source=measurement_complex(amplitude,0.0f);
    measurement_complex_t returned=measurement_complex_mul(source,t);
    measurement_complex_t channels[4]={source,returned,source,
        measurement_complex_mul(returned,measurement_complex(15.31f,0.03f))};
    float c=1.0f,s=0.0f;
    const float dc=profile.samples_per_cycle==64u?0.9951847267f:0.9238795325f;
    const float ds=profile.samples_per_cycle==64u?0.0980171403f:0.3826834324f;
    for(uint32_t n=0;n<HW_METROLOGY_SAMPLES_PER_BLOCK;n++)
    {
        uint16_t codes[4];
        for(unsigned j=0;j<4u;j++) codes[j]=raw(1.65f+channels[j].re*c-channels[j].im*s);
        words[3u*n]=hw_metrology_pack_word(codes[0],codes[1]);
        words[3u*n+1u]=hw_metrology_pack_word(codes[2],codes[3]);
        words[3u*n+2u]=hw_metrology_pack_word(raw(1.65f),raw(1.65f));
        const float next=c*dc-s*ds; s=c*ds+s*dc; c=next;
    }
    hw_metrology_analyze_block(words,HW_METROLOGY_SAMPLES_PER_BLOCK,&block);
    active=true; done=false; aborted=false; ticks=0u;
    return BSP_STATUS_BUSY;
}
static bsp_status_t step_capture(uint32_t time,void *user)
{
    (void)time;(void)user;
    if(active && (injection&3u)) aborted=true;
    if(aborted || (!(injection&8u) && ++ticks>=3u)) {active=false;done=true;}
    return BSP_STATUS_OK;
}
static bool is_active(void *u){(void)u;return active;}
static bool is_done(void *u){(void)u;return done;}
static bool dumpable(void *u){(void)u;return done&&!aborted&&!(injection&4u);}
static const hw_metrology_block_t *get_block(void *u){(void)u;return &block;}
static hw_metrology_measure_error_t error(void *u)
{(void)u;return injection&4u ? HW_METROLOGY_MEASURE_ERR_DMA : HW_METROLOGY_MEASURE_OK;}
static void ack(void *u){(void)u;done=false;active=false;}
static bsp_status_t abort_capture(void *u){(void)u;aborted=true;return BSP_STATUS_BUSY;}
static void snapshot(app_cal_capture_snapshot_t *state,void *u)
{
    (void)u;
    *state=(app_cal_capture_snapshot_t){.factory_allowed=(injection&3u)==0u,
        .transfer_safe=!active,.safety_blocks=(injection&3u)|(!active?HW_SAFETY_BLOCK_RANGE:0u),
        .temperature_valid=true,.temperature_mC=25000};
}
static bsp_status_t write_byte(uint8_t value,void *u)
{
    (void)u;
    if(active || output_size==sizeof(output)) return BSP_STATUS_BUSY;
    output[output_size++]=value; return BSP_STATUS_OK;
}
int main(void)
{
#ifdef _WIN32
    (void)_setmode(_fileno(stdin),_O_BINARY);
    (void)_setmode(_fileno(stdout),_O_BINARY);
#endif
    memset(nor,0xff,sizeof(nor));
    const app_cal_session_io_t session={.start_capture=start_capture,.step_capture=step_capture,
        .capture_active=is_active,.capture_done=is_done,.capture_dumpable=dumpable,.capture_block=get_block,
        .capture_error=error,.capture_acknowledge=ack,.capture_abort=abort_capture};
    const app_cal_capture_io_t io={.snapshot=snapshot,.try_write_byte=write_byte,.synthetic=true,.installation_supported=true,
        .device_uid={1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u}};
    app_io_workspace_init(&workspace);
    app_calibration_service_init(&calibration);
    app_calibration_service_attach_workspace(&calibration,&workspace);
    (void)app_calibration_service_load(&calibration,&flash_io,CAPACITY);
    if(app_cal_capture_init(&capture,&calibration,&session,&io,&clock_info,BSP_STATUS_OK)!=BSP_STATUS_OK) return 2;
    for(;;)
    {
        uint8_t header[3], data[144];
        if(fread(header,1u,3u,stdin)!=3u) break;
        const size_t count=(size_t)header[1]+((size_t)header[2]<<8u);
        if(count>sizeof(data)||fread(data,1u,count,stdin)!=count) return 3;
        if(header[0]==1u) for(size_t i=0;i<count;i++) app_cal_capture_receive_byte(&capture,data[i],now);
        else if(header[0]==2u && count==2u)
        {
            unsigned steps=(unsigned)data[0]+((unsigned)data[1]<<8u);
            while(steps--!=0u) app_cal_capture_step(&capture,++now);
        }
        else if(header[0]==3u && count==1u)
        { injection=data[0]; capture.clock_status=injection&16u?BSP_STATUS_ERROR:BSP_STATUS_OK; }
        else if(header[0]==4u && count==2u)
        {
            apply_pending((size_t)data[0]+((size_t)data[1]<<8u));
            injection=0u;active=false;done=false;now=0u;
            app_io_workspace_init(&workspace);app_calibration_service_init(&calibration);
            app_calibration_service_attach_workspace(&calibration,&workspace);
            (void)app_calibration_service_load(&calibration,&flash_io,CAPACITY);
            (void)app_cal_capture_init(&capture,&calibration,&session,&io,&clock_info,BSP_STATUS_OK);
        }
        else if(header[0]==5u && count>=3u)
        {
            size_t offset=(size_t)data[1]+((size_t)data[2]<<8u)+(size_t)data[0]*4096u;
            if(data[0]>1u || offset>=sizeof(nor) || count-3u>sizeof(nor)-offset) return 5;
            memcpy(nor+offset,data+3,count-3u);
        }
        else if(header[0]==6u && count==0u)
        {
            output[0]=(uint8_t)calibration.store.state;output[1]=(uint8_t)calibration.store.target_slot;
            output[2]=(uint8_t)pending_kind;output[3]=(uint8_t)workspace.owner;
            const uint32_t values[4]={(uint32_t)calibration.store.program_offset,(uint32_t)pending_size,
                operations,app_calibration_service_active_sequence(&calibration)};
            memcpy(output+4,values,sizeof(values));output_size=20u;
        }
        else if(header[0]==8u && count==0u)
        {
            calibration.workflow.request.standard.type=APP_CAL_STANDARD_LOAD;
            calibration.workflow.request.standard.z_ohms=measurement_complex(600.0f,0.0f);
            const hw_metrology_measure_request_t request={.frequency=HW_EXCITATION_FREQ_1KHZ,
                .amplitude=HW_EXCITATION_AMP_100MVRMS,.range_id=HW_RANGE_ID_1K};
            (void)start_capture(&request,now,NULL);active=false;done=false;
            const measurement_cal_key_t key=measurement_cal_key(MEASUREMENT_CAL_HARDWARE_REV1,
                MEASUREMENT_CAL_MODEL_VERSION_CURRENT,HW_RANGE_ID_1K,HW_EXCITATION_FREQ_1KHZ,HW_EXCITATION_AMP_100MVRMS);
            measurement_calibrated_result_t result;
            const bsp_status_t status=measurement_cal_process_block(&block,
                app_calibration_service_active_set(&calibration),&key,false,&result);
            const uint32_t values[4]={(uint32_t)status,result.provenance.set_sequence,
                result.output_corrected?1u:0u,(uint32_t)result.provenance.source};
            memcpy(output,values,sizeof(values));
            memcpy(output+16,&result.result.impedance.z_ohms,sizeof(measurement_complex_t));output_size=24u;
        }
        else return 4;
        uint8_t size[2]={(uint8_t)output_size,(uint8_t)(output_size>>8u)};
        (void)fwrite(size,1u,2u,stdout);(void)fwrite(output,1u,output_size,stdout);(void)fflush(stdout);
        output_size=0u;
    }
    return 0;
}
