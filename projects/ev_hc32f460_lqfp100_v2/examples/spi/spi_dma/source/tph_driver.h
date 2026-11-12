#ifndef __TPH_DRIVER_H__
#define __TPH_DRIVER_H__

#include "hc32_ll.h"
#include "tph_geometry.h"

/* M0333N 喷头参数定义 (600喷嘴 = 25组物理地址 * 4个分段(Section) * 6个数据域(Domain)) */

/* 硬件端口与引脚定义 (严格对应 MCU2.SchDoc) */
// 1. 电源使能
#define TPH_PWR_VP_PORT         (GPIO_PORT_B)
#define TPH_PWR_VP_PIN          (GPIO_PIN_05)   // TPH_ENABLE (VP 15V 加热源)
#define TPH_PWR_VL_PORT         (GPIO_PORT_B)
#define TPH_PWR_VL_PIN          (GPIO_PIN_06)   // VL_ENABLE  (VL 15V 逻辑源)

// 2. 打火脉冲使能 CE (High Active)
#define TPH_CE_PORT             (GPIO_PORT_B)
#define TPH_CE_PIN              (GPIO_PIN_15)

// 3. S1 ~ S4 数据分段采样/锁存引脚 (低电平采样，高电平锁存)
#define TPH_S1_PORT             (GPIO_PORT_C)
#define TPH_S1_PIN              (GPIO_PIN_15)
#define TPH_S2_PORT             (GPIO_PORT_H)
#define TPH_S2_PIN              (GPIO_PIN_01)
#define TPH_S3_PORT             (GPIO_PORT_H)
#define TPH_S3_PIN              (GPIO_PIN_00)
#define TPH_S4_PORT             (GPIO_PORT_C)
#define TPH_S4_PIN              (GPIO_PIN_14)

// 4. 与主控 BK7258 握手同步线
#define TPH_SYNC_IN_PORT        (GPIO_PORT_C)
#define TPH_SYNC_IN_PIN         (GPIO_PIN_13)   // BK_1 (打火同步信号输入)
#define TPH_READY_OUT_PORT      (GPIO_PORT_H)
#define TPH_READY_OUT_PIN       (GPIO_PIN_02)   // BK_2 (小华就绪状态输出)

// 喷头颜色掩码定义
#define COLOR_MASK_CYAN         (0x03)  // 青色 (D1, D2)
#define COLOR_MASK_MAGENTA      (0x0C)  // 品红 (D3, D4)
#define COLOR_MASK_YELLOW       (0x30)  // 黄色 (D5, D6)
#define COLOR_MASK_GREEN        (0x33)  // 绿色 (青色+黄色)
#define COLOR_MASK_ALL_BLACK    (0x3F)  // 三色全开 (黑色)

// 导出单列发送与打火函数
void tph_fire_raw_column_bytes(const uint8_t *p_column_100b);
/* 函数声明 */
void TPH_Init(void);
void TPH_PowerControl(en_functional_state_t enState);
void TPH_FireOneColumn(const uint8_t *pu8Data);

#endif /* __TPH_DRIVER_H__ */