#include "stm32f1xx_hal.h"
#include "usbd_core.h"
#include "usbd_conf.h"
#include "usbd_cdc.h"

PCD_HandleTypeDef hpcd_USB_FS;

/* -----------------------------------------------------------------------
 * Static allocator — single allocation for CDC class handle
 * ----------------------------------------------------------------------- */
void *USBD_static_malloc(uint32_t size)
{
    (void)size;
    static uint32_t mem[MAX_STATIC_ALLOC_SIZE / 4 + 1];
    return mem;
}

void USBD_static_free(void *p)
{
    (void)p;
}

/* -----------------------------------------------------------------------
 * PCD MSP: PA11=DM, PA12=DP, USB FS clock, interrupt
 * ----------------------------------------------------------------------- */
void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
    (void)hpcd;
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA11 = USB_DM, PA12 = USB_DP — input with pull-up per F1 RM */
    g.Pin   = GPIO_PIN_11 | GPIO_PIN_12;
    g.Mode  = GPIO_MODE_AF_INPUT;
    g.Pull  = GPIO_NOPULL;   /* D− must not be pulled up; external 1.5k on D+ handles enumeration */
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    __HAL_RCC_USB_CLK_ENABLE();

    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd)
{
    (void)hpcd;
    __HAL_RCC_USB_CLK_DISABLE();
}

/* -----------------------------------------------------------------------
 * PCD → USB Device Library callbacks
 * ----------------------------------------------------------------------- */
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_SetupStage((USBD_HandleTypeDef *)hpcd->pData, (uint8_t *)hpcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_DataOutStage((USBD_HandleTypeDef *)hpcd->pData, epnum,
                         hpcd->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_DataInStage((USBD_HandleTypeDef *)hpcd->pData, epnum,
                        hpcd->IN_ep[epnum].xfer_buff);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_SOF((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_SetSpeed((USBD_HandleTypeDef *)hpcd->pData, USBD_SPEED_FULL);
    USBD_LL_Reset((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_Suspend((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_Resume((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_IsoINIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_DevConnected((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_DevDisconnected((USBD_HandleTypeDef *)hpcd->pData);
}

/* -----------------------------------------------------------------------
 * USB Device Library → PCD interface
 * ----------------------------------------------------------------------- */
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
    hpcd_USB_FS.Instance            = USB;
    hpcd_USB_FS.Init.dev_endpoints  = 8;
    hpcd_USB_FS.Init.phy_itface     = PCD_PHY_EMBEDDED;
    hpcd_USB_FS.Init.speed          = PCD_SPEED_FULL;
    hpcd_USB_FS.Init.low_power_enable = 0;

    hpcd_USB_FS.pData = pdev;
    pdev->pData = &hpcd_USB_FS;

    HAL_PCD_Init(&hpcd_USB_FS);

    /* Packet Memory Area (PMA) allocation — total PMA = 512 bytes
     *   0x040: EP0 OUT (64 bytes)
     *   0x080: EP0 IN  (64 bytes)
     *   0x0C0: CDC IN  (EP1 IN, 64 bytes)
     *   0x100: CDC CMD (EP2 IN, 8 bytes notification)
     *   0x110: CDC OUT (EP1 OUT, 64 bytes) */
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x00,       PCD_SNG_BUF, 0x040);
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x80,       PCD_SNG_BUF, 0x080);
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, CDC_IN_EP,  PCD_SNG_BUF, 0x0C0);
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, CDC_CMD_EP, PCD_SNG_BUF, 0x100);
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, CDC_OUT_EP, PCD_SNG_BUF, 0x110);

    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
    HAL_PCD_DeInit((PCD_HandleTypeDef *)pdev->pData);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
    HAL_PCD_Start((PCD_HandleTypeDef *)pdev->pData);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
    HAL_PCD_Stop((PCD_HandleTypeDef *)pdev->pData);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                   uint8_t ep_type, uint16_t ep_mps)
{
    HAL_PCD_EP_Open((PCD_HandleTypeDef *)pdev->pData, ep_addr, ep_mps, ep_type);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_PCD_EP_Close((PCD_HandleTypeDef *)pdev->pData, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_PCD_EP_Flush((PCD_HandleTypeDef *)pdev->pData, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_PCD_EP_SetStall((PCD_HandleTypeDef *)pdev->pData, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_PCD_EP_ClrStall((PCD_HandleTypeDef *)pdev->pData, ep_addr);
    return USBD_OK;
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)pdev->pData;
    if (ep_addr & 0x80u)
        return hpcd->IN_ep[ep_addr & 0x7Fu].is_stall;
    return hpcd->OUT_ep[ep_addr & 0x7Fu].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{
    HAL_PCD_SetAddress((PCD_HandleTypeDef *)pdev->pData, dev_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                     uint8_t *pbuf, uint16_t size)
{
    HAL_PCD_EP_Transmit((PCD_HandleTypeDef *)pdev->pData, ep_addr, pbuf, size);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                           uint8_t *pbuf, uint16_t size)
{
    HAL_PCD_EP_Receive((PCD_HandleTypeDef *)pdev->pData, ep_addr, pbuf, size);
    return USBD_OK;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return HAL_PCD_EP_GetRxCount((PCD_HandleTypeDef *)pdev->pData, ep_addr);
}

void USBD_LL_Delay(uint32_t delay)
{
    HAL_Delay(delay);
}

/* Blue Pill: no discrete disconnect pin — DP pull-up is hardwired to 3.3V.
 * A 50 ms low-drive on PA12 before USB init handles re-enumeration (main.c). */
void HAL_PCDEx_SetConnectionState(PCD_HandleTypeDef *hpcd, uint8_t state)
{
    (void)hpcd;
    (void)state;
}
