#ifndef __USBD_CONF_H
#define __USBD_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USBD_MAX_NUM_INTERFACES       1U
#define USBD_MAX_NUM_CONFIGURATION    1U
#define USBD_MAX_STR_DESC_SIZ         0x100U
#define USBD_SUPPORT_USER_STRING_DESC 0U
#define USBD_SELF_POWERED             1U
#define USBD_DEBUG_LEVEL              0U

/* Static allocator — avoids heap dependency */
void *USBD_static_malloc(uint32_t size);
void  USBD_static_free(void *p);

#define MAX_STATIC_ALLOC_SIZE  140U   /* sizeof(USBD_CDC_HandleTypeDef) */

#define USBD_malloc    (uint32_t *)USBD_static_malloc
#define USBD_free      USBD_static_free
#define USBD_memset    memset
#define USBD_memcpy    memcpy

#define USBD_UsrLog(...)
#define USBD_ErrLog(...)
#define USBD_DbgLog(...)

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CONF_H */
