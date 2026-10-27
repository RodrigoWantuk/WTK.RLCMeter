
#include "storage/lead_profile_store.h"
#include <stdlib.h>
#undef assert
#define assert(c) do { if(!(c)) { fprintf(stderr,"assertion failed at line %u: %s\n",(unsigned)__LINE__,#c); exit(1); } } while(0)
#include <stdio.h>
#include <string.h>
enum { CAPACITY=49152, SECTOR=4096, LEAD_A=8192, LEAD_B=12288, LEGACY=16384 };
static uint8_t flash[CAPACITY], baseline[CAPACITY];
static struct { bool pending, contention, unread; uint32_t address, size; const uint8_t *src; } io;
static bsp_status_t read_flash(uint32_t address, void *data, size_t size, void *user)
{
    (void)user; assert(address+size<=CAPACITY);
    if(io.unread) return BSP_STATUS_ERROR;
    memcpy(data,flash+address,size); return BSP_STATUS_OK;
}
static bsp_status_t erase(uint32_t address,uint32_t now,void *user)
{
    (void)now; (void)user;
    assert(address==LEAD_A || address==LEAD_B);
    if(io.contention || io.pending) return BSP_STATUS_BUSY;
    io.pending=true; io.address=address; io.size=SECTOR; io.src=NULL; return BSP_STATUS_OK;
}
static bsp_status_t program(uint32_t address,const void *data,size_t size,uint32_t now,void *user)
{
    (void)now; (void)user;
    assert(address>=LEAD_A && address+size<=LEGACY);
    if(io.contention || io.pending) return BSP_STATUS_BUSY;
    io.pending=true; io.address=address; io.size=(uint32_t)size; io.src=data; return BSP_STATUS_OK;
}
static bsp_status_t poll_flash(uint32_t now,void *user)
{
    (void)now; (void)user; assert(io.pending);
    for(uint32_t i=0;i<io.size;++i) {
        if(io.src==NULL) flash[io.address+i]=255u;
        else flash[io.address+i]&=io.src[i];
    }
    io.pending=false; return BSP_STATUS_OK;
}
static const storage_async_io_t callbacks={.read=read_flash,.erase_sector_start=erase,.program_start=program,.poll=poll_flash};
static measurement_lead_profile_t profile(void)
{
    measurement_lead_profile_t p={.factory_sequence=3u,.factory_crc32=0x12345678u,
        .hardware_revision=MEASUREMENT_CAL_HARDWARE_REV1,.model_version=MEASUREMENT_CAL_MODEL_VERSION_CURRENT,
        .coverage=7u,.enabled=true};
    p.uid[0]=1u; p.frequencies[0].series_ohms.re=0.2f; return p;
}
static lead_profile_store_t boot(void)
{
    lead_profile_store_t s; io.pending=false;
    assert(lead_profile_store_init(&s,&callbacks,CAPACITY)==BSP_STATUS_OK);
    assert(lead_profile_store_load(&s)==BSP_STATUS_OK); return s;
}
static void finish(lead_profile_store_t *s)
{
    for(unsigned i=0u;i<30u && lead_profile_store_busy(s);++i)
        (void)lead_profile_store_step(s,100u+i);
    assert(s->state==LEAD_STORE_DONE);
    assert(memcmp(flash,baseline,LEAD_A)==0);
    assert(memcmp(flash+LEGACY,baseline+LEGACY,CAPACITY-LEGACY)==0);
}
int main(void)
{
    const uint32_t capacities[]={32768u,40960u,2097152u,4194304u,8388608u,16777216u};
    for(unsigned i=0u;i<sizeof(capacities)/sizeof(capacities[0]);++i) {
        const uint32_t c=capacities[i];storage_partition_t resource,old,la,lb;
        assert(storage_layout_partition(c,STORAGE_PARTITION_RESOURCE_PACK,&resource));
        assert(storage_layout_partition(c,STORAGE_PARTITION_CALIBRATION_A,&old) && old.start==c-32768u);
        if(c<40960u) assert(!storage_layout_partition(c,STORAGE_PARTITION_LEAD_A,&la));
        else {
            assert(storage_layout_partition(c,STORAGE_PARTITION_LEAD_A,&la));
            assert(storage_layout_partition(c,STORAGE_PARTITION_LEAD_B,&lb));
            assert(resource.start==0u && resource.size==c-40960u && la.start==resource.size &&
                   la.start+la.size==lb.start && lb.start+lb.size==old.start);
            assert(!storage_layout_contains(&resource,la.start,1u));
            assert(storage_layout_contains(&lb,lb.start+4095u,1u) && !storage_layout_contains(&lb,lb.start+4095u,2u));
        }
    }
    memset(flash,0xa5,sizeof(flash)); memset(flash+LEAD_A,255,2*SECTOR); memcpy(baseline,flash,sizeof(flash));
    measurement_lead_profile_t p=profile(); lead_profile_store_t s=boot();
    assert(!s.active_valid && s.loaded);
    assert(lead_profile_store_save(&s,&p,0u)==BSP_STATUS_OK);
    io.contention=true; assert(lead_profile_store_step(&s,1u)==BSP_STATUS_BUSY);
    assert(s.state==LEAD_STORE_ERASE_START); io.contention=false;
    p.frequencies[0].series_ohms.re=0.7f; finish(&s);
    assert(s.active.sequence==1u && s.active.frequencies[0].series_ohms.re==0.2f);
    s=boot(); assert(s.active_valid && s.active.sequence==1u);
    uint8_t stable[2*SECTOR]; memcpy(stable,flash+LEAD_A,sizeof(stable));
    /* Every cooperative power-cut boundary admits the old or complete new record. */
    for(unsigned cut=0u;cut<12u;++cut) {
        memcpy(flash+LEAD_A,stable,sizeof(stable)); s=boot();
        assert(lead_profile_store_save(&s,&p,0u)==BSP_STATUS_OK);
        for(unsigned i=0u;i<cut && lead_profile_store_busy(&s);++i) (void)lead_profile_store_step(&s,i);
        s=boot(); assert(s.active_valid); assert(s.active.sequence==1u || s.active.sequence==2u);
        if(s.active.sequence==2u) assert(s.active.frequencies[0].series_ohms.re==0.7f);
    }
    memcpy(flash+LEAD_A,stable,sizeof(stable)); s=boot();
    assert(lead_profile_store_save(&s,&p,0u)==BSP_STATUS_OK); finish(&s);
    assert(s.active.sequence==2u);
    flash[LEAD_B+12u]^=1u; s=boot(); assert(s.active.sequence==1u); /* Header CRC covers sequence. */
    p.enabled=false; p.cleared=true;
    assert(lead_profile_store_save(&s,&p,0u)==BSP_STATUS_OK); finish(&s);
    s=boot(); assert(s.active.cleared && !s.active.enabled && s.active.sequence==2u);
    assert(lead_profile_store_save(&s,&p,UINT32_MAX-100u)==BSP_STATUS_OK);
    assert(lead_profile_store_step(&s,4900u)==BSP_STATUS_TIMEOUT && !s.commit_uncertain);
    s=boot(); io.unread=true; assert(lead_profile_store_load(&s)==BSP_STATUS_ERROR);
    assert(!s.loaded && lead_profile_store_save(&s,&p,0u)==BSP_STATUS_ERROR); io.unread=false;
    s=boot(); assert(lead_profile_store_save(&s,&p,0u)==BSP_STATUS_OK);
    while(s.state!=LEAD_STORE_VERIFY_COMMIT) (void)lead_profile_store_step(&s,1u);
    io.unread=true; assert(lead_profile_store_step(&s,2u)==BSP_STATUS_ERROR);
    assert(s.commit_uncertain && s.active.sequence==2u); io.unread=false;
    s=boot(); assert(s.active.sequence==3u); /* Commit happened; acknowledgement was lost. */
    uint8_t frame[LEAD_PROFILE_FRAME_BYTES]; assert(lead_profile_encode(&p,true,frame));
    measurement_lead_profile_t out; assert(lead_profile_decode(frame,true,&out));
    frame[48]^=1u; assert(!lead_profile_decode(frame,true,&out));
    assert(memcmp(flash+LEGACY,baseline+LEGACY,CAPACITY-LEGACY)==0);
    puts("lead persistence: power cuts, contention, immutable frame, tombstone, CRC, uncertainty and legacy preservation passed");
    return 0;
}
