/**
 *******************************************************************************
 * @file  flash_conf.h
 * @brief This file contains emulate eeprom resource configure.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2024-11-08       CDT             First version
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

#ifndef __FLASH_CONF_H
#define __FLASH_CONF_H

/* C binding of definitions if building with C++ compiler */
#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "hc32_ll_efm.h"

/**
 * @addtogroup FLASH_EMULATE_EEPROM_APPLICATION
 * @{
 */

/**
 * @addtogroup FLASH_EMULATE_EEPROM_CONFIG
 * @{
 */

/*******************************************************************************
 * Global type definitions ('typedef')
*******************************************************************************/
typedef uint32_t    ee_data_addr_size_t;
typedef uint16_t    ee_addr_size_t;
typedef uint8_t     ee_data_size_t;
/*******************************************************************************
 * Global pre-processor symbols/macros ('#define')
 ******************************************************************************/
/**
 * @defgroup EEPROM_Start_Addr  EEPROM Start Addr
 * @{
 */
/* EEPROM start address in Flash. Can be modified by the user */
#define EEPROM_START_ADDR           (0x0007A000UL)
/**
 * @}
 */

/**
 * @defgroup EEPROM_Single_Sector_Size  EEPROM Single Sector Size
 * @{
 */
/* Used to confirm single eeprom sector space size. Can be modified by the user */
#define EEPROM_SECTOR_SIZE          (EFM_SECTOR_SIZE)
/**
 * @}
 */

/**
 * @addtogroup EEPROM_Virtual_Address_Capacity EEPEOM Virtual Address Capacity
 * @{
 */
/**
 * maximum eeprom total capacity, Cannot exceed what the capacity of a single sector divided by
 * the size of ee_data_addr_size_t divided by 2.
 * Reducing capacity increases eeprom life.Based on actual, Can be modified by the user.
*/
#define EE_CAPACITY                 (ee_addr_size_t)(EEPROM_SECTOR_SIZE / sizeof(ee_data_addr_size_t) / 2UL)
/**
 * @}
 */

/*******************************************************************************
 * Global variable definitions ('extern')
 ******************************************************************************/

/*******************************************************************************
  Global function prototypes (definition in C source)
 ******************************************************************************/
/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif /* __FLASH_CONF_H */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
