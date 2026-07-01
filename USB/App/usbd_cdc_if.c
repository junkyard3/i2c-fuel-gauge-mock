#include "usbd_cdc_if.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

extern USBD_HandleTypeDef hUsbDeviceFS;

/* -----------------------------------------------------------------------
 * TX ring buffer — written by printf/_write, drained by cdc_flush()
 * Size must be power-of-two for masking.
 * ----------------------------------------------------------------------- */
#define CDC_TX_BUF_SIZE  4096u

static uint8_t  s_tx_buf[CDC_TX_BUF_SIZE];
static uint32_t s_tx_in  = 0;  /* written by _write */
static uint32_t s_tx_out = 0;  /* consumed by cdc_flush */

/* RX buffer (incoming host → device; ignored for the sniffer) */
static uint8_t s_rx_buf[CDC_DATA_FS_MAX_PACKET_SIZE];

/* -----------------------------------------------------------------------
 * CDC interface callbacks
 * ----------------------------------------------------------------------- */
static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, s_rx_buf);
    return USBD_OK;
}

static int8_t CDC_DeInit_FS(void)
{
    return USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    (void)length;
    static USBD_CDC_LineCodingTypeDef lc = {115200, 0, 0, 8};

    switch (cmd) {
    case CDC_SET_LINE_CODING:
        lc.bitrate     = (uint32_t)(pbuf[0] | (pbuf[1]<<8) | (pbuf[2]<<16) | (pbuf[3]<<24));
        lc.format      = pbuf[4];
        lc.paritytype  = pbuf[5];
        lc.datatype    = pbuf[6];
        break;

    case CDC_GET_LINE_CODING:
        pbuf[0] = (uint8_t) lc.bitrate;
        pbuf[1] = (uint8_t)(lc.bitrate >>  8);
        pbuf[2] = (uint8_t)(lc.bitrate >> 16);
        pbuf[3] = (uint8_t)(lc.bitrate >> 24);
        pbuf[4] = lc.format;
        pbuf[5] = lc.paritytype;
        pbuf[6] = lc.datatype;
        break;

    default:
        break;
    }
    return USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t *buf, uint32_t *len)
{
    (void)buf;
    (void)len;
    /* Re-arm receive for the next packet */
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, s_rx_buf);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
};

/* -----------------------------------------------------------------------
 * cdc_flush — send up to one CDC packet (64 bytes) per call.
 * USBD_CDC_TransmitPacket checks TxState internally and returns USBD_BUSY
 * if a transfer is still in progress; we just retry on the next call.
 * Data reaches PMA synchronously inside TransmitPacket, so advancing
 * s_tx_out immediately after USBD_OK is safe.
 * ----------------------------------------------------------------------- */
void cdc_flush(void)
{
    static uint32_t tx_start_ms = 0;

    uint32_t in  = s_tx_in;
    uint32_t out = s_tx_out;
    if (in == out)
        return;

    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (hcdc == NULL)
        return;

    if (hcdc->TxState != 0) {
        /* TxState stuck: Windows stopped polling the IN endpoint after idle.
         * Force-clear after 500 ms so we can retry. */
        if (HAL_GetTick() - tx_start_ms > 500) {
            hcdc->TxState = 0;
            HAL_PCD_EP_Flush((PCD_HandleTypeDef *)hUsbDeviceFS.pData, CDC_IN_EP);
        }
        return;
    }

    uint32_t avail;
    if (in > out)
        avail = in - out;
    else
        avail = CDC_TX_BUF_SIZE - out;

    /* Cap at 63 bytes — avoids the ZLP path in USBD_CDC_DataIn that triggers
     * an extra IN transaction which the host may not service when idle. */
    if (avail >= CDC_DATA_FS_MAX_PACKET_SIZE)
        avail = CDC_DATA_FS_MAX_PACKET_SIZE - 1;

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, &s_tx_buf[out], (uint16_t)avail);
    if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) == USBD_OK) {
        s_tx_out = (out + avail) & (CDC_TX_BUF_SIZE - 1);
        tx_start_ms = HAL_GetTick();
    }
}

int cdc_tx_pending(void)
{
    return s_tx_in != s_tx_out;
}

/* Discard all unsent bytes.  Any USB transfer that is already in-flight
 * finishes cleanly (its data is already in PMA); we just stop queuing more. */
void cdc_tx_discard(void)
{
    s_tx_in = s_tx_out;
}

/* -----------------------------------------------------------------------
 * _write syscall — called by printf / fwrite to stdout/stderr
 * ----------------------------------------------------------------------- */
int _write(int fd, char *buf, int len)
{
    (void)fd;
    for (int i = 0; i < len; i++) {
        uint32_t next = (s_tx_in + 1) & (CDC_TX_BUF_SIZE - 1);
        if (next == s_tx_out)
            break; /* TX buffer full — drop remaining */
        s_tx_buf[s_tx_in] = (uint8_t)buf[i];
        s_tx_in = next;
    }
    return len;
}
