#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

#define LED_PIN   GPIO_PIN_13
#define LED_PORT  GPIOC

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
