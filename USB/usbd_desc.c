#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_conf.h"

#define USBD_VID                    0x0483U
#define USBD_PID                    0x5740U
#define USBD_LANGID_STRING          0x0409U
#define USBD_MANUFACTURER_STRING    "I2C Sniffer"
#define USBD_PRODUCT_FS_STRING      "I2C Sniffer VCP"
#define USBD_CONFIGURATION_STRING   "VCP Config"
#define USBD_INTERFACE_STRING       "VCP Interface"

static uint8_t *USBD_VCP_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_VCP_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_VCP_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_VCP_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_VCP_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_VCP_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_VCP_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);

USBD_DescriptorsTypeDef VCP_Desc = {
    USBD_VCP_DeviceDescriptor,
    USBD_VCP_LangIDStrDescriptor,
    USBD_VCP_ManufacturerStrDescriptor,
    USBD_VCP_ProductStrDescriptor,
    USBD_VCP_SerialStrDescriptor,
    USBD_VCP_ConfigStrDescriptor,
    USBD_VCP_InterfaceStrDescriptor,
};

__ALIGN_BEGIN static uint8_t USBD_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
    0x12,
    USB_DESC_TYPE_DEVICE,
    0x00, 0x02,       /* bcdUSB 2.00 */
    0x02,             /* bDeviceClass: CDC */
    0x02,             /* bDeviceSubClass */
    0x00,             /* bDeviceProtocol */
    USB_MAX_EP0_SIZE, /* bMaxPacketSize0 */
    LOBYTE(USBD_VID), HIBYTE(USBD_VID),
    LOBYTE(USBD_PID), HIBYTE(USBD_PID),
    0x00, 0x02,       /* bcdDevice rel. 2.00 */
    USBD_IDX_MFC_STR,
    USBD_IDX_PRODUCT_STR,
    USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION,
};

__ALIGN_BEGIN static uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING),
};

__ALIGN_BEGIN static uint8_t USBD_StringSerial[USB_SIZ_STRING_SERIAL] __ALIGN_END = {
    USB_SIZ_STRING_SERIAL,
    USB_DESC_TYPE_STRING,
};

__ALIGN_BEGIN static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        uint8_t nibble = (value >> 28) & 0xFu;
        pbuf[2 * i] = (nibble < 10) ? (nibble + '0') : (nibble + 'A' - 10);
        pbuf[2 * i + 1] = 0;
        value <<= 4;
    }
}

static void Get_SerialNum(void)
{
    uint32_t id0 = *(uint32_t *)DEVICE_ID1 + *(uint32_t *)DEVICE_ID3;
    uint32_t id1 = *(uint32_t *)DEVICE_ID2;
    IntToUnicode(id0, &USBD_StringSerial[2],  8);
    IntToUnicode(id1, &USBD_StringSerial[18], 4);
}

static uint8_t *USBD_VCP_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_DeviceDesc);
    return USBD_DeviceDesc;
}

static uint8_t *USBD_VCP_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_LangIDDesc);
    return USBD_LangIDDesc;
}

static uint8_t *USBD_VCP_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_VCP_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_PRODUCT_FS_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_VCP_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = USB_SIZ_STRING_SERIAL;
    Get_SerialNum();
    return USBD_StringSerial;
}

static uint8_t *USBD_VCP_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_CONFIGURATION_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_VCP_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_INTERFACE_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}
