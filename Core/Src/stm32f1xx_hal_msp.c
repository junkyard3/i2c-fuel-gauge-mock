#include "main.h"

void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    /* Disable JTAG, keep SWD — frees PB3/PB4/PA15 if needed later */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

