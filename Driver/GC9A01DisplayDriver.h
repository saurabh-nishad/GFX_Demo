#ifndef __DISPLAY_H
#define __DISPLAY_H

/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @brief LCD_OrientationTypeDef
 *  Possible values of Display Orientation
 */
#define GC9A01_ORIENTATION_LANDSCAPE         ((uint32_t)0x00) /* Landscape orientation choice of LCD screen  */
#define GC9A01_ORIENTATION_PORTRAIT        ((uint32_t)0x01) /* Portrait orientation choice of LCD screen */

void     touchgfxDisplayDriverTransmitBlock(const uint8_t* pixels, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
int      touchgfxDisplayDriverTransmitionActive(void);
int      touchgfxDisplayDriverShouldTransferBlock(uint16_t bottom);
void     GC9A01DisplayDriver_DMACallback(void);
void     GC9A01DisplayDriver_Init(void);
void     GC9A01DisplayDriver_DisplayOn(void);
void     GC9A01DisplayDriver_DisplayReset(void);
void     GC9A01DisplayDriver_SetOrientation(uint32_t orientation);
void     waveshareModule_Init(void);
void     GC9A01DisplayDriver_DisplayInit(void);

void     DisplayDriver_TransferCompleteCallback(void);

void test(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
