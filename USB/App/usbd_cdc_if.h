#ifndef __USBD_CDC_IF_H
#define __USBD_CDC_IF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_cdc.h"

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

/* Called from main loop — flushes pending TX data over USB */
void cdc_flush(void);

/* Returns non-zero if unsent bytes remain in the TX ring buffer */
int cdc_tx_pending(void);

/* Discard all unsent bytes — call at session boundary to unblock new output */
void cdc_tx_discard(void);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CDC_IF_H */
