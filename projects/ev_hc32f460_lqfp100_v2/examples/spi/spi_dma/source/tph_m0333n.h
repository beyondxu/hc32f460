/**
 *******************************************************************************
 * @file  spi/spi_dma/source/tph_m0333n.h
 * @brief This file contains the including files of main routine.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
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



#ifndef __TPH_M0333N_H__
#define __TPH_M0333N_H__

#include "hc32_ll.h"
#include "tph_geometry.h"
#include <string.h>

/* ========================================================================== */
/* 1. 硬件引脚映射 (严格对照 HC32F460 与 X2 接口原理图)                       */
/* ========================================================================== */

// 电源使能
#define PIN_VL_EN_PORT          (GPIO_PORT_B)
#define PIN_VL_EN_PIN           (GPIO_PIN_06)   // PB7: 控制 VL (15V 逻辑电)

#define PIN_TPH_EN_PORT         (GPIO_PORT_B)
#define PIN_TPH_EN_PIN          (GPIO_PIN_05)   // PB6: 控制 VP (15V 发热电)

// 芯片加热使能 CE
#define PIN_CE_PORT             (GPIO_PORT_B)
#define PIN_CE_PIN              (GPIO_PIN_15)   // PB15: CE (加热脉冲, 高有效)

// 地址线 A1 ~ A7
#define PIN_A1_PORT             (GPIO_PORT_B)
#define PIN_A1_PIN              (GPIO_PIN_02)   // PB2
#define PIN_A2_PORT             (GPIO_PORT_B)
#define PIN_A2_PIN              (GPIO_PIN_10)   // PB10
#define PIN_A3_PORT             (GPIO_PORT_B)
#define PIN_A3_PIN              (GPIO_PIN_01)   // PB1
#define PIN_A4_PORT             (GPIO_PORT_B)
#define PIN_A4_PIN              (GPIO_PIN_12)   // PB12
#define PIN_A5_PORT             (GPIO_PORT_B)
#define PIN_A5_PIN              (GPIO_PIN_14)   // PB14
#define PIN_A6_PORT             (GPIO_PORT_B)
#define PIN_A6_PIN              (GPIO_PIN_13)   // PB13
#define PIN_A7_PORT             (GPIO_PORT_B)
#define PIN_A7_PIN              (GPIO_PIN_00)   // PB0

// 分段锁存线 S1 ~ S4 (低电平采样，高电平锁存)
#define PIN_S1_PORT             (GPIO_PORT_C)
#define PIN_S1_PIN              (GPIO_PIN_15)   // PC15
#define PIN_S2_PORT             (GPIO_PORT_H)
#define PIN_S2_PIN              (GPIO_PIN_01)   // PH1
#define PIN_S3_PORT             (GPIO_PORT_H)
#define PIN_S3_PIN              (GPIO_PIN_00)   // PH0
#define PIN_S4_PORT             (GPIO_PORT_C)
#define PIN_S4_PIN              (GPIO_PIN_14)   // PC14

// 区域数据线 D1 ~ D6 (高电平喷墨)
#define PIN_D1_PORT             (GPIO_PORT_A)
#define PIN_D1_PIN              (GPIO_PIN_00)   // PA0
#define PIN_D2_PORT             (GPIO_PORT_A)
#define PIN_D2_PIN              (GPIO_PIN_02)   // PA2
#define PIN_D3_PORT             (GPIO_PORT_A)
#define PIN_D3_PIN              (GPIO_PIN_01)   // PA1
#define PIN_D4_PORT             (GPIO_PORT_A)
#define PIN_D4_PIN              (GPIO_PIN_10)   // PA10
#define PIN_D5_PORT             (GPIO_PORT_A)
#define PIN_D5_PIN              (GPIO_PIN_08)   // PA8
#define PIN_D6_PORT             (GPIO_PORT_A)
#define PIN_D6_PIN              (GPIO_PIN_03)   // PA3

/* 单列点阵字节数：25 地址 * 4 分区 = 100 字节 */

/* 测试颜色通道选择枚举 */
typedef enum {
    COLOR_TEST_CYAN     = 0x01, // 仅喷青色 (D1, D2 开启)
    COLOR_TEST_MAGENTA  = 0x02, // 仅喷品红 (D3, D4 开启)
    COLOR_TEST_YELLOW   = 0x04, // 仅喷黄色 (D5, D6 开启)
    COLOR_TEST_ALL      = 0x07  // 三色全喷
} tph_color_channel_t;

/* 驱动接口函数声明 */
void TPH_M0333N_Init(void);
void TPH_M0333N_Power(en_functional_state_t enState);
void TPH_FireOneColumn(const uint8_t *pColumn100B);
void TPH_Test_SingleColor_Fire(tph_color_channel_t color);

#endif /* __TPH_M0333N_H__ */