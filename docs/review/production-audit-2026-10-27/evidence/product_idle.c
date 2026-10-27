/* Reuse host-only controller fixtures; no hardware action. */
#define main existing_test_main
#include AUDIT_SOURCE
#undef main
#include <assert.h>
int main(void)
{
    fake_io_t fake={0};
    app_product_t product;
    assert(init_product(&product,&fake)==BSP_STATUS_OK);
    app_product_inputs_t inputs=inputs_ready();
    inputs.range_disabled=true;
    inputs.safety_result.measure_allowed=false;
    inputs.safety_result.blocker_flags=HW_SAFETY_BLOCK_RANGE;
    inputs.safety_result.primary_blocker=HW_SAFETY_BLOCKED_RANGE;
    boot_to_ready(&product,&inputs);
    assert(product.view.state==UI_PRODUCT_STATE_READY);
    app_settings_t settings=*app_settings_service_current(&g_settings);
    settings.backlight_timeout=APP_BACKLIGHT_TIMEOUT_15S;
    assert(app_settings_service_set(&g_settings,&settings)==BSP_STATUS_OK);
    const bsp_clock_summary_t clock={.source=BSP_CLOCK_SOURCE_HSE_PLL,.sysclk_hz=72000000u,.hse_ready=true};
    app_product_step(&product,&inputs,&clock,BSP_STATUS_OK,20000);
    assert(!product.backlight_sleeping && product.last_activity_ms==20000);
    printf("CONFIRMED idle timeout suppressed: READY=%u range_block=%lu timeout_s=%u now_ms=20000 last_activity=%lu sleeping=%u\n",
        product.view.state,(unsigned long)inputs.safety_result.blocker_flags,
        (unsigned)settings.backlight_timeout,(unsigned long)product.last_activity_ms,product.backlight_sleeping);
    return 0;
}
