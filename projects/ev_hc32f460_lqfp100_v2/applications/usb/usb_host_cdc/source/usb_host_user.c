/**
 *******************************************************************************
 * @file  usb/usb_host_cdc/source/usb_host_user.c
 * @brief user application layer.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2024-11-08       CDT             Remove unused code in host_user_userinput() function
                                    Redesign the CDC data tx and rx application demo
 @endverbatim
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include <string.h>
#include <stdio.h>
#include "usb_host_user.h"
#include "usb_host_cdc_class.h"
#include "usb_host_cdc_ctrl.h"
#include "usb_host_driver.h"
#include "ev_hc32f460_lqfp100_v2_bsp.h"

/**
 * @addtogroup HC32F460_DDL_Applications
 * @{
 */

/**
 * @addtogroup USB_Host_Cdc
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define DATA_TEST_LEN               (200UL)
#define DATA_TEST_TIMEOUT           (0xFFFFFUL)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/
extern usb_core_instance          usb_app_instance;
extern USBH_HOST                  usb_app_host;

/*  Points to the DEVICE_PROP structure of current device */
/*  The purpose of this register is to speed up the execution */
usb_host_user_callback_func USR_cb = {
    &host_user_init,
    &host_user_denint,
    &host_user_devattached,
    &host_user_devreset,
    &host_user_devdisconn,
    &host_user_overcurrent,
    &host_user_devspddetected,
    &host_user_devdescavailable,
    &host_user_devaddrdistributed,
    &host_user_cfgdescavailable,
    &host_user_mfcstring,
    &host_user_productstring,
    &host_user_serialnum,
    &host_user_enumcompl,
    &host_user_userinput,
    &host_user_cdc_app,
    &host_user_devunsupported,
    &host_user_unrecoverederror

};

uint8_t USB_HOST_USER_AppState = USBH_USR_STATE_INIT;

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
static void usb_host_cdc_receivedata_Callback(uint8_t *pbuf, uint32_t len);

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
/* USBH_USR_Private_Constants */
const static char *MSG_DEV_ATTACHED     = "> Device Attached\r\n";
const static char *MSG_DEV_DISCONNECTED = "> Device Disconnected\r\n";
const static char *MSG_DEV_ENUMERATED   = "> Enumeration completed\r\n";
const static char *MSG_DEV_FULLSPEED    = "> Full speed device detected\r\n";
const static char *MSG_DEV_LOWSPEED     = "> Low speed device detected\r\n";
const static char *MSG_DEV_ERROR        = "> Device fault\r\n";

const static char *MSG_MSC_CLASS        = "> Mass storage device connected\r\n";
const static char *MSG_IF_CDC_CLASS     = "> Interface CDC\r\n";
const static char *MSG_IF_ACM_CLASS     = "> Interface ACM\r\n";
const static char *MSG_VENDOR_CLASS     = "> Device Vendor Specific\r\n";
const static char *MSG_HID_CLASS        = "> HID device connected\r\n";
const static char *MSG_UNREC_ERROR      = "> UNRECOVERED ERROR STATE\r\n";

static uint8_t m_u8CdcInterfaceInitFailCount = 0U;

static uint8_t m_u8UsbHostAppState = USBH_USR_STATE_INIT;
static uint8_t m_au8TestTxBuf[DATA_TEST_LEN];
static uint8_t m_au8TestRxBuf[DATA_TEST_LEN];
static uint8_t m_u8TestRxCompletet = 0U;
static uint32_t m_u32TestRxLen = 0UL;
static uint32_t m_u32Timeout = 0UL;

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  Displays the message on terminal for host lib initialization
 * @param  None
 * @retval None
 */
void host_user_init(void)
{
    static uint8_t startup = 0U;
    uint32_t i;

    if (startup == 0U) {
        startup = 1U;
#if (LL_PRINT_ENABLE == DDL_ON)
        DDL_Printf("> USB Host library started.\r\n");
        DDL_Printf("     USB Host Library v2.1.0\r\n");
#endif
        /* Init tx buffer */
        for (i = 0UL; i < DATA_TEST_LEN; i++) {
            m_au8TestTxBuf[i] = (uint8_t)i;
        }
    }
}

/**
 * @brief  Displays the message on terminal via DDL_Printf
 * @param  None
 * @retval None
 */
void host_user_devattached(void)
{
    m_u8CdcInterfaceInitFailCount = 0U;
    m_u8UsbHostAppState = USBH_USR_STATE_INIT;
#if (LL_PRINT_ENABLE == DDL_ON)
    DDL_Printf(MSG_DEV_ATTACHED);
#endif
}

/**
 * @brief  host_user_unrecoverederror
 * @param  None
 * @retval None
 */
void host_user_unrecoverederror(void)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    DDL_Printf(MSG_UNREC_ERROR);
#endif
}

/**
 * @brief  Device disconnect event
 * @param  None
 * @retval None
 */
void host_user_devdisconn(void)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    DDL_Printf(MSG_DEV_DISCONNECTED);
#endif
}

/**
 * @brief  USBH_USR_ResetUSBDevice
 * @param  None
 * @retval None
 */
void host_user_devreset(void)
{
    /* callback for USB-Reset */
}

/**
 * @brief  host_user_devspddetected
 * @param  [in] DeviceSpeed      USB speed
 * @retval None
 */
void host_user_devspddetected(uint8_t DeviceSpeed)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    if (DeviceSpeed == PRTSPD_FULL_SPEED) {
        DDL_Printf(MSG_DEV_FULLSPEED);
    } else if (DeviceSpeed == PRTSPD_LOW_SPEED) {
        DDL_Printf(MSG_DEV_LOWSPEED);
    } else {
        DDL_Printf(MSG_DEV_ERROR);
    }
#endif
}

/**
 * @brief  host_user_devdescavailable
 * @param  [in] DeviceDesc       device descriptor
 * @retval None
 */
void host_user_devdescavailable(void *DeviceDesc)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    usb_host_devdesc_typedef *hs;
    hs = DeviceDesc;
    DDL_Printf("VID : %04lXh\r\n", (uint32_t)(*hs).idVendor);
    DDL_Printf("PID : %04lXh\r\n", (uint32_t)(*hs).idProduct);
#endif
}

/**
 * @brief  host_user_devaddrdistributed
 * @param  None
 * @retval None
 */
void host_user_devaddrdistributed(void)
{
}

/**
 * @brief  host_user_cfgdescavailable
 * @param  [in] cfgDesc          Configuration desctriptor
 * @param  [in] itfDesc          Interface desctriptor
 * @param  [in] epDesc           Endpoint desctriptor
 * @retval None
 */
void host_user_cfgdescavailable(usb_host_cfgdesc_typedef *cfgDesc,
                                usb_host_itfdesc_typedef *itfDesc,
                                USB_HOST_EPDesc_TypeDef *epDesc)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    usb_host_itfdesc_typedef *id;

    id = itfDesc;
    if ((*id).bInterfaceClass  == 0x08U) {
        DDL_Printf(MSG_MSC_CLASS);
    } else if ((*id).bInterfaceClass  == 0x03U) {
        DDL_Printf(MSG_HID_CLASS);
    } else if ((*id).bInterfaceClass  == 0x02U) {
        DDL_Printf(MSG_IF_ACM_CLASS);
    } else if ((*id).bInterfaceClass  == 0x0AU) {
        DDL_Printf(MSG_IF_CDC_CLASS);
    } else if ((*id).bInterfaceClass  == 0xFFU) {
        DDL_Printf(MSG_VENDOR_CLASS);
    } else {
        ;
    }
#endif
}

/**
 * @brief  Displays the message on terminal for Manufacturer String
 * @param  [in] ManufacturerString
 * @retval None
 */
void host_user_mfcstring(void *ManufacturerString)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    DDL_Printf("Manufacturer : %s\r\n", (char *)ManufacturerString);
#endif
}

/**
 * @brief  Displays the message on terminal for product String
 * @param  [in] ProductString
 * @retval None
 */
void host_user_productstring(void *ProductString)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    DDL_Printf("Product : %s\r\n", (char *)ProductString);
#endif
}

/**
 * @brief  Displays the message on terminal for SerialNum_String
 * @param  [in] SerialNumString
 * @retval None
 */
void host_user_serialnum(void *SerialNumString)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    DDL_Printf("Serial Number : %s\r\n", (char *)SerialNumString);
#endif
}

/**
 * @brief  User response request is displayed to ask application jump to class
 * @param  None
 * @retval None
 */
void host_user_enumcompl(void)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    /* Enumeration complete */
    DDL_Printf(MSG_DEV_ENUMERATED);
#endif
    UserCb.Receive = usb_host_cdc_receivedata_Callback;
}

/**
 * @brief  Device is not supported
 * @param  None
 * @retval None
 */
void host_user_devunsupported(void)
{
    m_u8CdcInterfaceInitFailCount++;
#if (LL_PRINT_ENABLE == DDL_ON)
    if (1U == m_u8CdcInterfaceInitFailCount) {
        DDL_Printf("> Device is not a standard CDC ACM device.\r\n");
    } else if (2U == m_u8CdcInterfaceInitFailCount) {
        DDL_Printf("> Device is not a standard CDC ACM device.\r\n");
        DDL_Printf("> Try to Parse Vendor CDC device.\r\n");
    } else if (3U == m_u8CdcInterfaceInitFailCount) {
        DDL_Printf("> Device is not supported.\r\n");
    } else {
        ;
    }
#endif
}

/**
 * @brief  User Action for application state entry
 * @param  None
 * @retval HOST_USER_STATUS
 */
HOST_USER_STATUS host_user_userinput(void)
{
    HOST_USER_STATUS usbh_usr_status;

    usbh_usr_status = USER_NONE_RESP;

    return usbh_usr_status;
}

/**
 * @brief  Over Current Detected on VBUS
 * @param  None
 * @retval None
 */
void host_user_overcurrent(void)
{
#if (LL_PRINT_ENABLE == DDL_ON)
    DDL_Printf("Overcurrent detected.\r\n");
#endif
}

/**
 * @brief  CDC receive data callback.
 * @param  [in] pbuf    pointer to data received.
 * @param  [in] len     data received len.
 * @retval None
 */
static void usb_host_cdc_receivedata_Callback(uint8_t *pbuf, uint32_t len)
{
    if (0U == m_u8TestRxCompletet) {
        if ((m_u32TestRxLen + len) <= DATA_TEST_LEN) {
            (void)memcpy(&m_au8TestRxBuf[m_u32TestRxLen], pbuf, len);
        }
        m_u32TestRxLen += len;
        if (m_u32TestRxLen >= DATA_TEST_LEN) {
            m_u8TestRxCompletet = 1U;
        }
    }
}

/**
 * @brief  Demo application for cdc
 * @param  None
 * @retval None
 */
int host_user_cdc_app(void)
{
    switch (m_u8UsbHostAppState) {
        case USBH_USR_STATE_INIT:
            /* line config */
            (void)memset(&CDC_SetLineCode, 0, sizeof(CDC_SetLineCode));
            CDC_SetLineCode.b.dwDTERate = 115200;
            CDC_SetLineCode.b.bDataBits = 8;//8 bit data
            CDC_SetLineCode.b.bCharFormat = 0;//1 stop bit
            CDC_SetLineCode.b.bParityType = 0;//none
#if (LL_PRINT_ENABLE == DDL_ON)
            DDL_Printf("Set vcp linecfg: 115200bps, 8N1\r\n");
#endif
            usb_host_cdc_issue_setlinecoding(&usb_app_instance, &usb_app_host);

            usb_host_cdc_enable_receive(&usb_app_instance);

#if (LL_PRINT_ENABLE == DDL_ON)
            DDL_Printf("Please short the CDC device's UART TX and RX, then press host's K1 to test\r\n");
#endif
            m_u8UsbHostAppState = USBH_USR_STATE_KEY_WAIT;
            break;

        case USBH_USR_STATE_KEY_WAIT:
            if (SET == BSP_KEY_GetStatus(BSP_KEY_1)) {
                m_u8UsbHostAppState = USBH_USR_STATE_TX_DATA;
            }
            break;

        case USBH_USR_STATE_TX_DATA:
            m_u32TestRxLen = 0UL;
            m_u8TestRxCompletet = 0U;

            BSP_LED_Off(LED_BLUE);
            BSP_LED_Off(LED_RED);
            (void)memset(m_au8TestRxBuf, 0, DATA_TEST_LEN);
            usb_host_cdc_senddata(m_au8TestTxBuf, DATA_TEST_LEN);
#if (LL_PRINT_ENABLE == DDL_ON)
            DDL_Printf("CDC tx %ld bytes data\r\n", DATA_TEST_LEN);
#endif
            m_u32Timeout = 0UL;
            m_u8UsbHostAppState = USBH_USR_STATE_RX_WAIT;
            break;

        case USBH_USR_STATE_RX_WAIT:
            if (0U != m_u8TestRxCompletet) {
#if (LL_PRINT_ENABLE == DDL_ON)
                DDL_Printf("CDC rx %ld bytes data\r\n", m_u32TestRxLen);
#endif
                if ((0 == memcmp(m_au8TestTxBuf, m_au8TestRxBuf, DATA_TEST_LEN)) && \
                    (DATA_TEST_LEN == m_u32TestRxLen)) {
                    BSP_LED_On(LED_BLUE);
                    BSP_LED_Off(LED_RED);
#if (LL_PRINT_ENABLE == DDL_ON)
                    DDL_Printf("CDC Test passed\r\n");
#endif
                } else {
                    BSP_LED_On(LED_RED);
                    BSP_LED_Off(LED_BLUE);
#if (LL_PRINT_ENABLE == DDL_ON)
                    DDL_Printf("CDC Test failed\r\n");
#endif
                }
                m_u8UsbHostAppState = USBH_USR_STATE_KEY_WAIT;
            } else {
                m_u32Timeout++;
                if (m_u32Timeout >= DATA_TEST_TIMEOUT) {
                    BSP_LED_On(LED_RED);
                    BSP_LED_Off(LED_BLUE);
#if (LL_PRINT_ENABLE == DDL_ON)
                    DDL_Printf("CDC rx %ld bytes data\r\n", m_u32TestRxLen);
                    DDL_Printf("CDC Test timeout\r\n");
#endif
                    m_u8UsbHostAppState = USBH_USR_STATE_KEY_WAIT;
                }
            }
            break;

        default:
            break;
    }

    return ((int)0);
}

/**
 * @brief  Deint User state and associated variables
 * @param  None
 * @retval None
 */
void host_user_denint(void)
{
    /* uset state deinit */
}

/**
 * @}
 */

/**
 * @}
 */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
