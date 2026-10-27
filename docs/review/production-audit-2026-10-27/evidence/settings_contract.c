/* Audit-only synthetic IO reproduces real W25Q accepted-start BUSY semantics.
 * No physical Flash transaction. Production source is unmodified. */
#include "app/app_settings_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { CAPACITY = 65536 };
static unsigned char data[CAPACITY];
static unsigned erase_calls, poll_calls;
static bool pending, accepted_busy;
static bsp_status_t read_bytes(uint32_t address, void *dst, size_t size, void *user)
{
    (void)user;
    assert(size <= CAPACITY && address <= CAPACITY - size);
    memcpy(dst, data + address, size);
    return BSP_STATUS_OK;
}
static bsp_status_t erase_bytes(uint32_t address, uint32_t now, void *user)
{
    (void)now; (void)user; ++erase_calls;
    if (pending) return BSP_STATUS_BUSY;
    assert(address <= CAPACITY - 4096);
    memset(data + address, 255, 4096); pending = true;
    return accepted_busy ? BSP_STATUS_BUSY : BSP_STATUS_OK;
}
static bsp_status_t program_bytes(uint32_t address, const void *src, size_t size,
                                  uint32_t now, void *user)
{
    (void)now; (void)user;
    if (pending) return BSP_STATUS_BUSY;
    assert(size <= CAPACITY && address <= CAPACITY - size);
    const unsigned char *input = src;
    for (size_t i = 0; i < size; ++i) data[address+i] &= input[i];
    pending = true;
    return accepted_busy ? BSP_STATUS_BUSY : BSP_STATUS_OK;
}
static bsp_status_t poll_bytes(uint32_t now, void *user)
{
    (void)now; (void)user; ++poll_calls; pending = false;
    return BSP_STATUS_OK;
}
static const app_settings_store_io_t io = {
    .read=read_bytes, .erase_sector_start=erase_bytes,
    .program_start=program_bytes, .poll=poll_bytes, .user=NULL
};
static void setup(app_settings_service_t *settings, bool busy)
{
    memset(data,255,sizeof data); erase_calls=0; poll_calls=0; pending=false; accepted_busy=busy;
    assert(app_settings_service_init(settings,&io,CAPACITY)==BSP_STATUS_OK);
    assert(app_settings_service_load(settings,NULL)==BSP_STATUS_OK);
    app_settings_t value=app_settings_defaults(); value.brightness_percent=60;
    assert(app_settings_service_set(settings,&value)==BSP_STATUS_OK);
    assert(app_settings_service_save_start(settings,0)==BSP_STATUS_OK);
}
int main(void)
{
    app_settings_service_t settings,reloaded;
    setup(&settings,true);
    for (uint32_t now=1; now<=128; ++now) (void)app_settings_service_step(&settings,now);
    assert(settings.state==APP_SETTINGS_STORE_ERASE_START && poll_calls==0 && erase_calls==128);
    printf("CONFIRMED accepted-start BUSY stall: erase_calls=%u poll_calls=%u state=%u\n",
        erase_calls,poll_calls,(unsigned)settings.state);

    setup(&settings,false);
    (void)app_settings_service_step(&settings,1);
    app_settings_t later=settings.current; later.brightness_percent=65;
    assert(app_settings_service_set(&settings,&later)==BSP_STATUS_OK);
    for (uint32_t now=2; now<64 && app_settings_service_busy(&settings); ++now)
        (void)app_settings_service_step(&settings,now);
    assert(settings.save_failed && settings.current.brightness_percent==65);
    assert(app_settings_service_init(&reloaded,&io,CAPACITY)==BSP_STATUS_OK);
    assert(app_settings_service_load(&reloaded,NULL)==BSP_STATUS_OK);
    assert(reloaded.current.brightness_percent==60);
    printf("CONFIRMED edit-during-save mismatch: current=%u persisted_after_reload=%u failed=%u\n",
        settings.current.brightness_percent,reloaded.current.brightness_percent,settings.save_failed);

    setup(&settings,false);
    for (uint32_t now=1; now<64 && app_settings_service_busy(&settings); ++now)
        (void)app_settings_service_step(&settings,now);
    assert(!settings.save_failed && settings.persisted_valid);
    const uint32_t address=settings.slots[(unsigned)settings.active_slot].start;
    data[address+12] ^= 0x20u;
    assert(app_settings_service_init(&reloaded,&io,CAPACITY)==BSP_STATUS_OK);
    assert(app_settings_service_load(&reloaded,NULL)==BSP_STATUS_OK);
    assert(reloaded.persisted_valid && reloaded.persisted_sequence==33);
    printf("CONFIRMED sequence header outside CRC: changed_sequence=%lu accepted_valid=%u\n",
        (unsigned long)reloaded.persisted_sequence,reloaded.persisted_valid);
    return 0;
}
