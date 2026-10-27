#include "storage/lead_profile_store.h"
#include "storage/storage_crc32.h"
#include <string.h>
#define LEAD_MAGIC UINT32_C(0x3150444c)
#define LEAD_COMMIT UINT32_C(0x4c454144)
static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8u); }
static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i=0u; i<4u; ++i) p[i]=(uint8_t)(v>>(8u*i));
}
static uint16_t get16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8u)); }
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8u)|((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);
}
static uint32_t checksum(const uint8_t *p)
{
    uint32_t crc=storage_crc32_update(STORAGE_CRC32_INIT,p,44u);
    return storage_crc32_update(crc,p+48u,48u)^STORAGE_CRC32_XOR_OUT;
}
static void putfloat(uint8_t *p, float f)
{
    uint32_t bits;
    _Static_assert(sizeof(float)==sizeof(bits),"lead record requires binary32");
    memcpy(&bits,&f,sizeof(bits)); put32(p,bits);
}
static float getfloat(const uint8_t *p)
{
    const uint32_t bits=get32(p); float f; memcpy(&f,&bits,sizeof(f)); return f;
}
bool lead_profile_encode(const measurement_lead_profile_t *profile, bool committed, uint8_t frame[LEAD_PROFILE_FRAME_BYTES])
{
    if (frame==NULL || !measurement_lead_profile_valid(profile)) return false;
    memset(frame,0,LEAD_PROFILE_FRAME_BYTES);
    put32(frame,LEAD_MAGIC); put16(frame+4u,1u); put16(frame+6u,48u);
    put16(frame+8u,LEAD_PROFILE_FRAME_BYTES); put16(frame+10u,48u);
    put32(frame+12u,profile->sequence); put32(frame+16u,profile->factory_sequence);
    put32(frame+20u,profile->factory_crc32); put32(frame+24u,profile->hardware_revision);
    put16(frame+28u,profile->model_version); frame[30]=profile->coverage;
    frame[31]=(uint8_t)(4u|(profile->enabled?1u:0u)|(profile->cleared?2u:0u)); /* Engineering, not qualified. */
    memcpy(frame+32u,profile->uid,12u);
    for (unsigned i=0u; i<3u; ++i)
    {
        const measurement_lead_frequency_t *f=&profile->frequencies[i];
        putfloat(frame+48u+i*16u,f->series_ohms.re); putfloat(frame+52u+i*16u,f->series_ohms.im);
        putfloat(frame+56u+i*16u,f->shunt_siemens.re); putfloat(frame+60u+i*16u,f->shunt_siemens.im);
    }
    put32(frame+44u,checksum(frame)); put32(frame+96u,committed?LEAD_COMMIT:UINT32_MAX);
    return true;
}
bool lead_profile_decode(const uint8_t frame[LEAD_PROFILE_FRAME_BYTES], bool require_commit, measurement_lead_profile_t *profile)
{
    if (frame==NULL || profile==NULL || get32(frame)!=LEAD_MAGIC || get16(frame+4u)!=1u ||
        get16(frame+6u)!=48u || get16(frame+8u)!=LEAD_PROFILE_FRAME_BYTES || get16(frame+10u)!=48u ||
        (frame[31]&4u)==0u || (frame[31]&0xf8u)!=0u || get32(frame+44u)!=checksum(frame) ||
        (require_commit && get32(frame+96u)!=LEAD_COMMIT)) return false;
    measurement_lead_profile_t candidate={.sequence=get32(frame+12u),.factory_sequence=get32(frame+16u),
        .factory_crc32=get32(frame+20u),.hardware_revision=get32(frame+24u),.model_version=get16(frame+28u),
        .coverage=frame[30],.enabled=(frame[31]&1u)!=0u,.cleared=(frame[31]&2u)!=0u};
    memcpy(candidate.uid,frame+32u,12u);
    for (unsigned i=0u; i<3u; ++i)
        candidate.frequencies[i]=(measurement_lead_frequency_t){
            .series_ohms={getfloat(frame+48u+i*16u),getfloat(frame+52u+i*16u)},
            .shunt_siemens={getfloat(frame+56u+i*16u),getfloat(frame+60u+i*16u)}};
    if (!measurement_lead_profile_valid(&candidate)) return false;
    *profile=candidate; return true;
}
bsp_status_t lead_profile_store_init(lead_profile_store_t *store, const storage_async_io_t *io, uint32_t capacity)
{
    if (store==NULL || io==NULL || io->read==NULL || io->erase_sector_start==NULL || io->program_start==NULL || io->poll==NULL)
        return BSP_STATUS_INVALID_ARG;
    *store=(lead_profile_store_t){.io=*io};
    if (!storage_layout_partition(capacity,STORAGE_PARTITION_LEAD_A,&store->slots[0]) ||
        !storage_layout_partition(capacity,STORAGE_PARTITION_LEAD_B,&store->slots[1])) return BSP_STATUS_NOT_SUPPORTED;
    store->initialized=true; return BSP_STATUS_OK;
}
bool lead_profile_store_busy(const lead_profile_store_t *store)
{
    return store!=NULL && store->state>=LEAD_STORE_ERASE_START && store->state<=LEAD_STORE_VERIFY_COMMIT;
}
bsp_status_t lead_profile_store_load(lead_profile_store_t *store)
{
    if (store==NULL || !store->initialized) return BSP_STATUS_INVALID_ARG;
    if (lead_profile_store_busy(store)) return BSP_STATUS_BUSY;
    store->active_valid=false;
    store->loaded=false;
    bsp_status_t error=BSP_STATUS_OK;
    bool nonempty_invalid=false;
    for (unsigned i=0u; i<2u; ++i)
    {
        measurement_lead_profile_t candidate;
        const bsp_status_t read=store->io.read(store->slots[i].start,store->verify,LEAD_PROFILE_FRAME_BYTES,store->io.user);
        if (read!=BSP_STATUS_OK) { error=read; continue; }
        if (!lead_profile_decode(store->verify,true,&candidate))
        {
            for (unsigned j=0u; j<LEAD_PROFILE_FRAME_BYTES; ++j)
                nonempty_invalid=nonempty_invalid || store->verify[j]!=255u;
            continue;
        }
        if (!store->active_valid || (candidate.sequence!=store->active.sequence &&
            (uint32_t)(candidate.sequence-store->active.sequence)<0x80000000u))
        {
            store->active=candidate; store->active_slot=(uint8_t)i; store->active_valid=true;
        }
    }
    store->loaded=error==BSP_STATUS_OK;
    store->last_error=error!=BSP_STATUS_OK?error:(!store->active_valid && nonempty_invalid?BSP_STATUS_ERROR:BSP_STATUS_OK);
    return store->last_error; /* Erased absence is normal; unread slots cannot be overwritten as empty. */
}
bsp_status_t lead_profile_store_save(lead_profile_store_t *store, const measurement_lead_profile_t *profile, uint32_t now_ms)
{
    if (store==NULL || !store->initialized || !measurement_lead_profile_valid(profile)) return BSP_STATUS_INVALID_ARG;
    if (lead_profile_store_busy(store)) return BSP_STATUS_BUSY;
    if (!store->loaded) return BSP_STATUS_ERROR;
    measurement_lead_profile_t snapshot=*profile;
    snapshot.sequence=store->active_valid?store->active.sequence+1u:1u;
    if (!lead_profile_encode(&snapshot,false,store->frame)) return BSP_STATUS_INVALID_ARG;
    store->target_slot=store->active_valid?(uint8_t)(store->active_slot^1u):0u;
    store->started_ms=now_ms; store->commit_uncertain=false; store->last_error=BSP_STATUS_OK;
    store->state=LEAD_STORE_ERASE_START; return BSP_STATUS_OK;
}
static bsp_status_t fail(lead_profile_store_t *store, bsp_status_t error)
{
    store->commit_uncertain=store->state>=LEAD_STORE_COMMIT_START;
    store->state=LEAD_STORE_ERROR; store->last_error=error; return error;
}
bsp_status_t lead_profile_store_step(lead_profile_store_t *store, uint32_t now_ms)
{
    if (store==NULL || !store->initialized) return BSP_STATUS_INVALID_ARG;
    if (!lead_profile_store_busy(store)) return store->state==LEAD_STORE_ERROR?store->last_error:BSP_STATUS_OK;
    if ((uint32_t)(now_ms-store->started_ms)>=5000u) return fail(store,BSP_STATUS_TIMEOUT);
    const uint32_t address=store->slots[store->target_slot].start;
    bsp_status_t status=BSP_STATUS_OK;
    switch (store->state)
    {
    case LEAD_STORE_ERASE_START:
        status=store->io.erase_sector_start(address,now_ms,store->io.user);
        if (status==BSP_STATUS_OK) store->state=LEAD_STORE_ERASE_WAIT;
        break;
    case LEAD_STORE_ERASE_WAIT:
        status=store->io.poll(now_ms,store->io.user);
        if (status==BSP_STATUS_OK) store->state=LEAD_STORE_WRITE_START;
        break;
    case LEAD_STORE_WRITE_START:
        status=store->io.program_start(address,store->frame,LEAD_PROFILE_COMMIT_OFFSET,now_ms,store->io.user);
        if (status==BSP_STATUS_OK) store->state=LEAD_STORE_WRITE_WAIT;
        break;
    case LEAD_STORE_WRITE_WAIT:
        status=store->io.poll(now_ms,store->io.user);
        if (status==BSP_STATUS_OK) store->state=LEAD_STORE_VERIFY;
        break;
    case LEAD_STORE_VERIFY:
        status=store->io.read(address,store->verify,LEAD_PROFILE_FRAME_BYTES,store->io.user);
        if (status==BSP_STATUS_OK)
        {
            if (memcmp(store->frame,store->verify,LEAD_PROFILE_COMMIT_OFFSET)!=0 || get32(store->verify+96u)!=UINT32_MAX)
                return fail(store,BSP_STATUS_ERROR);
            store->state=LEAD_STORE_COMMIT_START;
        }
        break;
    case LEAD_STORE_COMMIT_START:
        put32(store->frame+96u,LEAD_COMMIT);
        status=store->io.program_start(address+96u,store->frame+96u,4u,now_ms,store->io.user);
        if (status==BSP_STATUS_OK) store->state=LEAD_STORE_COMMIT_WAIT;
        break;
    case LEAD_STORE_COMMIT_WAIT:
        status=store->io.poll(now_ms,store->io.user);
        if (status==BSP_STATUS_OK) store->state=LEAD_STORE_VERIFY_COMMIT;
        break;
    case LEAD_STORE_VERIFY_COMMIT:
        status=store->io.read(address,store->verify,LEAD_PROFILE_FRAME_BYTES,store->io.user);
        if (status==BSP_STATUS_OK)
        {
            measurement_lead_profile_t verified;
            if (memcmp(store->frame,store->verify,LEAD_PROFILE_FRAME_BYTES)!=0 || !lead_profile_decode(store->verify,true,&verified))
                return fail(store,BSP_STATUS_ERROR);
            store->active=verified; store->active_valid=true; store->active_slot=store->target_slot;
            store->state=LEAD_STORE_DONE;
        }
        break;
    default: return fail(store,BSP_STATUS_ERROR);
    }
    if (status!=BSP_STATUS_OK && status!=BSP_STATUS_BUSY) return fail(store,status);
    return lead_profile_store_busy(store)?BSP_STATUS_BUSY:BSP_STATUS_OK;
}
