#include "stm32g0xx_hal.h"
#include "DCS.h"
#include "main.h"
#include "GC9A01DisplayDriver.h"
#include <assert.h>

extern SPI_HandleTypeDef hspi1;
extern DMA_HandleTypeDef hdma_spi1_tx;
extern TIM_HandleTypeDef htim6;

volatile uint16_t TE = 0;

static uint32_t displayOrientation;

//Signal TE interrupt to TouchGFX
void touchgfxSignalVSync(void);

static void Display_DCS_Send(uint8_t command)
{
  // Reset the nCS pin
  DISPLAY_CSX_GPIO_Port->BRR = DISPLAY_CSX_Pin;
  // Set the DCX pin
  DISPLAY_DCX_GPIO_Port->BRR = DISPLAY_DCX_Pin;

  // Send the command
  *((__IO uint8_t*)&hspi1.Instance->DR) = command;

  // Wait until the bus is not busy before changing configuration
  while(((hspi1.Instance->SR) & SPI_FLAG_BSY) != RESET);

  // Reset the DCX pin
  DISPLAY_DCX_GPIO_Port->BSRR = DISPLAY_DCX_Pin;

  // Set the nCS
  DISPLAY_CSX_GPIO_Port->BSRR = DISPLAY_CSX_Pin;
}

static void Display_DCS_Send_With_Data(uint8_t command, uint8_t* data, uint8_t size)
{
  // Reset the nCS pin
  DISPLAY_CSX_GPIO_Port->BRR = DISPLAY_CSX_Pin;
  // Set the DCX pin
  DISPLAY_DCX_GPIO_Port->BRR = DISPLAY_DCX_Pin;

  *((__IO uint8_t*)&hspi1.Instance->DR) = command;

  // Wait until the bus is not busy before changing configuration
  while(((hspi1.Instance->SR) & SPI_FLAG_BSY) != RESET);
  DISPLAY_DCX_GPIO_Port->BSRR = DISPLAY_DCX_Pin;

  while (size > 0U)
  {
    *((__IO uint8_t*)&hspi1.Instance->DR) = *data;
    data++;
    size--;
    /* Wait until TXE flag is set to send data */
    while(((hspi1.Instance->SR) & SPI_FLAG_TXE) != SPI_FLAG_TXE);
  }

  // Wait until the bus is not busy before changing configuration
  while(((hspi1.Instance->SR) & SPI_FLAG_BSY) != RESET);

  // Set the nCS
  DISPLAY_CSX_GPIO_Port->BSRR = DISPLAY_CSX_Pin;
}

void GC9A01DisplayDriver_DisplayOn(void)
{
  // Display ON
  Display_DCS_Send(DCS_SET_DISPLAY_ON);
  HAL_Delay(100);
}

void Display_OFF(void)
{
  // Display OFF
  Display_DCS_Send(DCS_SET_DISPLAY_OFF);
  HAL_Delay(100);
}

static uint16_t old_x0=0xFFFF, old_x1=0xFFFF, old_y0=0xFFFF, old_y1=0xFFFF;

void Display_Set_Area(uint16_t x0, uint16_t y0,
                      uint16_t x1, uint16_t y1)
{
  uint8_t arguments[4];

  // Set columns, if changed
  if (x0 != old_x0 || x1 != old_x1)
  {
    arguments[0] = x0 >> 8;
    arguments[1] = x0 & 0xFF;
    arguments[2] = x1 >> 8;
    arguments[3] = x1 & 0xFF;
    Display_DCS_Send_With_Data(DCS_SET_COLUMN_ADDRESS, arguments, 4);

    old_x0 = x0;
    old_x1 = x1;
  }

  // Set rows, if changed
  if (y0 != old_y0 || y1 != old_y1)
  {
    arguments[0] = y0 >> 8;
    arguments[1] = y0 & 0xFF;
    arguments[2] = y1 >> 8;
    arguments[3] = y1 & 0xFF;
    Display_DCS_Send_With_Data(DCS_SET_ROW_ADDRESS, arguments, 4);

    old_y0 = y0;
    old_y1 = y1;
  }
}

void test()
{
  uint16_t i, j;
  uint8_t color1 = 0xF0;
  uint8_t color2 = 0x0F;
  uint8_t cmd = 0x2C;

  Display_Set_Area(0, 0, 239, 239);

  HAL_GPIO_WritePin(DISPLAY_DCX_GPIO_Port, DISPLAY_DCX_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DISPLAY_CSX_GPIO_Port, DISPLAY_CSX_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, (uint8_t *)&(cmd), 1, 500);

  // Reset the nCS pin
  // DISPLAY_CSX_GPIO_Port->BRR = DISPLAY_CSX_Pin;
  HAL_GPIO_WritePin(DISPLAY_CSX_GPIO_Port, DISPLAY_CSX_Pin, GPIO_PIN_RESET);
  // Set the DCX pin
  // DISPLAY_DCX_GPIO_Port->BRR = DISPLAY_DCX_Pin;
  HAL_GPIO_WritePin(DISPLAY_DCX_GPIO_Port, DISPLAY_DCX_Pin, GPIO_PIN_SET);

	for(i = 0; i < 240; i++){
		for(j = 0; j < 240; j++){
			HAL_SPI_Transmit(&hspi1, (uint8_t *)&(color1), 1, 500);
			HAL_SPI_Transmit(&hspi1, (uint8_t *)&(color2), 1, 500);
		}
	 }

   GC9A01DisplayDriver_DisplayOn();
}

volatile uint8_t IsTransmittingBlock_;
void Display_Bitmap(const uint16_t *bitmap, uint16_t posx, uint16_t posy, uint16_t sizex, uint16_t sizey)
{
  IsTransmittingBlock_ = 1;
  __HAL_SPI_ENABLE(&hspi1); // Enables SPI peripheral
  uint8_t command = DCS_WRITE_MEMORY_START;

  // Define the display area
  Display_Set_Area(posx, posy, posx+sizex-1, posy+sizey-1);

  // Reset the nCS pin
  DISPLAY_CSX_GPIO_Port->BRR = DISPLAY_CSX_Pin;
  // Set the DCX pin
  DISPLAY_DCX_GPIO_Port->BRR = DISPLAY_DCX_Pin;

  *((__IO uint8_t*)&hspi1.Instance->DR) = command;

  // Wait until the bus is not busy before changing configuration
  while(((hspi1.Instance->SR) & SPI_FLAG_BSY) != RESET);
  DISPLAY_DCX_GPIO_Port->BSRR = DISPLAY_DCX_Pin;

  // Set the SPI in 16-bit mode to match endianess
  hspi1.Instance->CR2 = SPI_DATASIZE_16BIT;

  // Disable spi peripherals
  __HAL_SPI_DISABLE(&hspi1);
  __HAL_DMA_DISABLE(&hdma_spi1_tx);

  CLEAR_BIT(hspi1.Instance->CR2, SPI_CR2_LDMATX);

  /* Clear all flags */
  __HAL_DMA_CLEAR_FLAG(&hdma_spi1_tx, (DMA_FLAG_GI1 << (hdma_spi1_tx.ChannelIndex & 0x1cU)));

  /* Configure DMA Channel data length */
  hdma_spi1_tx.Instance->CNDTR = sizex*sizey;
  /* Configure DMA Channel destination address */
  hdma_spi1_tx.Instance->CPAR = (uint32_t)&hspi1.Instance->DR;

  /* Configure DMA Channel source address */
  hdma_spi1_tx.Instance->CMAR = (uint32_t)bitmap;

  /* Disable the transfer half complete interrupt */
  __HAL_DMA_DISABLE_IT(&hdma_spi1_tx, DMA_IT_HT);
  /* Enable the transfer complete interrupt */
  __HAL_DMA_ENABLE_IT(&hdma_spi1_tx, (DMA_IT_TC | DMA_IT_TE));

  /* Enable the Peripherals */
  __HAL_DMA_ENABLE(&hdma_spi1_tx);
  __HAL_SPI_ENABLE(&hspi1);

  /* Enable Tx DMA Request */
  SET_BIT(hspi1.Instance->CR2, SPI_CR2_TXDMAEN);
}

void GC9A01DisplayDriver_DisplayInit(void)
{
  uint8_t arguments[12];
  __HAL_SPI_ENABLE(&hspi1);

  /*********** Registers initialization ************/
  // ???
  Display_DCS_Send(DCS_INTER_REGISTER_ON2);
  arguments[0] = 0x14;
  Display_DCS_Send_With_Data(0xEB, arguments, 1);

  Display_DCS_Send(DCS_INTER_REGISTER_ON1);
  Display_DCS_Send(DCS_INTER_REGISTER_ON2);

  arguments[0] = 0x14;
  Display_DCS_Send_With_Data(0xEB, arguments, 1);

  arguments[0] = 0x40;
  Display_DCS_Send_With_Data(0x84, arguments, 1);

  arguments[0] = 0xFF;
  Display_DCS_Send_With_Data(0x85, arguments, 1);

  arguments[0] = 0xFF;
  Display_DCS_Send_With_Data(0x86, arguments, 1);

  arguments[0] = 0xFF;
  Display_DCS_Send_With_Data(0x87, arguments, 1);

  arguments[0] = 0x0A;
  Display_DCS_Send_With_Data(0x88, arguments, 1);

  arguments[0] = 0x21;
  Display_DCS_Send_With_Data(0x89, arguments, 1);

  arguments[0] = 0x00;
  Display_DCS_Send_With_Data(0x8A, arguments, 1);

  arguments[0] = 0x80;
  Display_DCS_Send_With_Data(0x8B, arguments, 1);

  arguments[0] = 0x01;
  Display_DCS_Send_With_Data(0x8C, arguments, 1);

  arguments[0] = 0x01;
  Display_DCS_Send_With_Data(0x8D, arguments, 1);

  arguments[0] = 0xFF;
  Display_DCS_Send_With_Data(0x8E, arguments, 1);

  arguments[0] = 0xFF;
  Display_DCS_Send_With_Data(0x8F, arguments, 1);

  arguments[0] = 0x00;
  arguments[1] = 0x20;
  Display_DCS_Send_With_Data(DCS_DISPLAY_FUNCTION_CONTROL, arguments, 2);

  arguments[0] = 0x68; // Set as vertical screen
  Display_DCS_Send_With_Data(DCS_SET_MEMORY_ACCESS_CONTROL, arguments, 1);

  arguments[0] = 0x05; // 16 bits per pixel
  Display_DCS_Send_With_Data(DCS_SET_PIXEL_FORMAT, arguments, 1);

  arguments[0] = 0x08;
  arguments[1] = 0x08;
  arguments[2] = 0x08;
  arguments[3] = 0x08;
  Display_DCS_Send_With_Data(0x90, arguments, 4);

  arguments[0] = 0x00;
  Display_DCS_Send_With_Data(0xBC, arguments, 1);

  arguments[0] = 0x06;
  Display_DCS_Send_With_Data(0xBD, arguments, 1);

  arguments[0] = 0x60;
  arguments[1] = 0x01;
  arguments[2] = 0x04;
  Display_DCS_Send_With_Data(0xFF, arguments, 3);

  arguments[0] = 0x13;
  Display_DCS_Send_With_Data(DCS_POWER_CONTROL_2, arguments, 1);

  arguments[0] = 0x13;
  Display_DCS_Send_With_Data(DCS_POWER_CONTROL_3, arguments, 1);

  arguments[0] = 0x22;
  Display_DCS_Send_With_Data(DCS_POWER_CONTROL_4, arguments, 1);

  arguments[0] = 0x11;
  Display_DCS_Send_With_Data(0xBE, arguments, 1);

  arguments[0] = 0x10;
  arguments[1] = 0x0E;
  Display_DCS_Send_With_Data(0xE1, arguments, 2);

  arguments[0] = 0x21;
  arguments[1] = 0x0C;
  arguments[2] = 0x02;
  Display_DCS_Send_With_Data(0xDF, arguments, 3);

  arguments[0] = 0x45;
  arguments[1] = 0x09;
  arguments[2] = 0x08;
  arguments[3] = 0x08;
  arguments[4] = 0x26;
  arguments[5] = 0x2A;
  Display_DCS_Send_With_Data(DCS_SET_GAMMA1, arguments, 6);
  Display_DCS_Send_With_Data(DCS_SET_GAMMA3, arguments, 6);

  arguments[0] = 0x43;
  arguments[1] = 0x70;
  arguments[2] = 0x72;
  arguments[3] = 0x36;
  arguments[4] = 0x37;
  arguments[5] = 0x6F;
  Display_DCS_Send_With_Data(DCS_SET_GAMMA2, arguments, 6);
  Display_DCS_Send_With_Data(DCS_SET_GAMMA4, arguments, 6);

  arguments[0] = 0x1B;
  arguments[1] = 0x0B;
  Display_DCS_Send_With_Data(0xED, arguments, 2);

  arguments[0] = 0x77;
  Display_DCS_Send_With_Data(0xAE, arguments, 1);

  arguments[0] = 0x63;
  Display_DCS_Send_With_Data(0xCD, arguments, 1);

  arguments[0] = 0x07;
  arguments[1] = 0x07;
  arguments[2] = 0x04;
  arguments[3] = 0x0E;
  arguments[4] = 0x0F;
  arguments[5] = 0x09;
  arguments[6] = 0x07;
  arguments[7] = 0x08;
  arguments[8] = 0x03;
  Display_DCS_Send_With_Data(0x70, arguments, 9);

  arguments[0] = 0x34; // 4 dot inversion
  Display_DCS_Send_With_Data(DCS_FRAME_RATE, arguments, 1);
  
  arguments[0] = 0x18;
  arguments[1] = 0x0D;
  arguments[2] = 0x71;
  arguments[3] = 0xED;
  arguments[4] = 0x70;
  arguments[5] = 0x70;
  arguments[6] = 0x18;
  arguments[7] = 0x0F;
  arguments[8] = 0x71;
  arguments[9] = 0xEF;
  arguments[10] = 0x70;
  arguments[11] = 0x70;
  Display_DCS_Send_With_Data(0x62, arguments, 12);

  arguments[0] = 0x18;
  arguments[1] = 0x11;
  arguments[2] = 0x71;
  arguments[3] = 0xF1;
  arguments[4] = 0x70;
  arguments[5] = 0x70;
  arguments[6] = 0x18;
  arguments[7] = 0x13;
  arguments[8] = 0x71;
  arguments[9] = 0xF3;
  arguments[10] = 0x70;
  arguments[11] = 0x70;
  Display_DCS_Send_With_Data(0x63, arguments, 12);

  arguments[0] = 0x28;
  arguments[1] = 0x29;
  arguments[2] = 0xF1;
  arguments[3] = 0x01;
  arguments[4] = 0xF1;
  arguments[5] = 0x00;
  arguments[6] = 0x07;
  Display_DCS_Send_With_Data(0x64, arguments, 7);

  arguments[0] = 0x3C;
  arguments[1] = 0x00;
  arguments[2] = 0xCD;
  arguments[3] = 0x67;
  arguments[4] = 0x45;
  arguments[5] = 0x45;
  arguments[6] = 0x10;
  arguments[7] = 0x00;
  arguments[8] = 0x00;
  arguments[9] = 0x00;
  Display_DCS_Send_With_Data(0x66, arguments, 10);

  arguments[0] = 0x00;
  arguments[1] = 0x3C;
  arguments[2] = 0x00;
  arguments[3] = 0x00;
  arguments[4] = 0x00;
  arguments[5] = 0x01;
  arguments[6] = 0x54;
  arguments[7] = 0x10;
  arguments[8] = 0x32;
  arguments[9] = 0x98;
  Display_DCS_Send_With_Data(0x67, arguments, 10);

  arguments[0] = 0x10;
  arguments[1] = 0x85;
  arguments[2] = 0x80;
  arguments[3] = 0x00;
  arguments[4] = 0x00;
  arguments[5] = 0x4E;
  arguments[6] = 0x00;
  Display_DCS_Send_With_Data(0x74, arguments, 7);

  arguments[0] = 0x3E;
  arguments[1] = 0x07;
  Display_DCS_Send_With_Data(0x98, arguments, 2);

  Display_DCS_Send(DCS_SET_TEAR_ON);
  Display_DCS_Send(DCS_ENTER_INVERT_MODE);

  Display_DCS_Send(DCS_EXIT_SLEEP_MODE);
  HAL_Delay(120);
}

void GC9A01DisplayDriver_DisplayReset(void)
{
  HAL_GPIO_WritePin(DISPLAY_RESET_GPIO_Port, DISPLAY_RESET_Pin, GPIO_PIN_SET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(DISPLAY_RESET_GPIO_Port, DISPLAY_RESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(DISPLAY_RESET_GPIO_Port, DISPLAY_RESET_Pin, GPIO_PIN_SET);
  HAL_Delay(100);
}

/**
  * @brief  Set the Display Orientation.
  * @param  orientation: GC9A01_ORIENTATION_PORTRAIT, GC9A01_ORIENTATION_LANDSCAPE
  *                        
  * @retval None
  */
void GC9A01DisplayDriver_SetOrientation(uint32_t orientation)
{
  uint8_t   parameter[6];

  displayOrientation = orientation;

  if(orientation == GC9A01_ORIENTATION_LANDSCAPE)
  {
    parameter[0] = 0XC8;     
  }
  else
  {
    parameter[0] = 0X68;     
  }
  Display_DCS_Send_With_Data(DCS_SET_MEMORY_ACCESS_CONTROL, parameter, 1);
}

void waveshareModule_Init(void)
{
  HAL_GPIO_WritePin(DISPLAY_DCX_GPIO_Port, DISPLAY_DCX_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(DISPLAY_CSX_GPIO_Port, DISPLAY_CSX_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(DISPLAY_RESET_GPIO_Port, DISPLAY_RESET_Pin, GPIO_PIN_SET);
  HAL_Delay(100);
}

void GC9A01DisplayDriver_Init(void)
{
  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

int touchgfxDisplayDriverTransmitActive(void)
{
  return IsTransmittingBlock_;
}

void touchgfxDisplayDriverTransmitBlock(const uint8_t* pixels, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  Display_Bitmap((uint16_t*)pixels, x, y, w, h);
}

void GC9A01DisplayDriver_DMACallback(void)
{
  /* Transfer Complete Interrupt management ***********************************/
  if ((0U != (DMA1->ISR & (DMA_FLAG_TC1))) && (0U != (hdma_spi1_tx.Instance->CCR & DMA_IT_TC)))
  {
    /* Disable the transfer complete and error interrupt */
    __HAL_DMA_DISABLE_IT(&hdma_spi1_tx, DMA_IT_TE | DMA_IT_TC);

    /* Clear the transfer complete flag */
    __HAL_DMA_CLEAR_FLAG(&hdma_spi1_tx, DMA_FLAG_TC1);

    IsTransmittingBlock_ = 0;

    // Wait until the bus is not busy before changing configuration
    // SPI is busy in communication or Tx buffer is not empty
    while(((hspi1.Instance->SR) & SPI_FLAG_BSY) != RESET) { }

    // Set the nCS
    DISPLAY_CSX_GPIO_Port->BSRR = DISPLAY_CSX_Pin;

    // Go back to 8-bit mode
    hspi1.Instance->CR2 = SPI_DATASIZE_8BIT;

    // Signal Transfer Complete to TouchGFX
    DisplayDriver_TransferCompleteCallback();
  }
    /* Transfer Error Interrupt management **************************************/
  else if ((0U != (DMA1->ISR & (DMA_FLAG_TC1))) && (0U != (hdma_spi1_tx.Instance->CCR & DMA_IT_TE)))
  {
    /* When a DMA transfer error occurs */
    /* A hardware clear of its EN bits is performed */
    /* Disable ALL DMA IT */
    __HAL_DMA_DISABLE_IT(&hdma_spi1_tx, (DMA_IT_TC | DMA_IT_HT | DMA_IT_TE));

    /* Clear all flags */
    __HAL_DMA_CLEAR_FLAG(&hdma_spi1_tx, DMA_FLAG_GI1 );

    assert(0);  // Halting program - Transfer Error Interrupt received.
  }
}

int touchgfxDisplayDriverShouldTransferBlock(uint16_t bottom)
{
  //return (bottom < getCurrentLine());
  // return (bottom < (TE > 0 ? 0xFFFF : ((__IO uint16_t)htim6.Instance->CNT)));
  return 0xFFFF;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    // Check which version of the timer triggered this callback
    if (htim == &htim6 )
    {
        TE++;
        touchgfxSignalVSync();
    }
}

