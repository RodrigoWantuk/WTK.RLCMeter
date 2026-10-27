#ifndef WTK_TEST_WATCHDOG_STM32F1XX_H
#define WTK_TEST_WATCHDOG_STM32F1XX_H
#include <stdint.h>
typedef struct { volatile uint32_t CSR; } wtk_test_rcc_t;
typedef struct { volatile uint32_t KR, PR, RLR, SR; } wtk_test_iwdg_t;
wtk_test_rcc_t *wtk_test_rcc(void);
wtk_test_iwdg_t *wtk_test_iwdg(void);
#define RCC wtk_test_rcc()
#define IWDG wtk_test_iwdg()
#define RCC_CSR_LSION UINT32_C(1)
#define RCC_CSR_LSIRDY UINT32_C(2)
#define IWDG_SR_PVU UINT32_C(1)
#define IWDG_SR_RVU UINT32_C(2)
#endif
