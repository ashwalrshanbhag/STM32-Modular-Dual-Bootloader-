/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32 Dual-Bootloader Main Source File
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "bootloader.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef void (*pFunction)(void);
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FLASH_APP_ADDR  0x08004000U
#define D_UART          &huart2
#define C_UART          &huart2
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
COM_InitTypeDef BspCOMInit;
CRC_HandleTypeDef hcrc;

/* USER CODE BEGIN PV */
uint8_t bl_rx_buffer[256];
UART_HandleTypeDef huart2 ;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CRC_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
void printmsg(char *format, ...);
void bootloader_uart_read_data(void);
void bootloader_jump_to_user_app(void);
__weak void bootloader_parse_and_execute(uint8_t *pBuffer);
/* USER CODE END PFP */

int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CRC_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  /* Replace long decorative banners with concise strings */
  char boot_banner[] = "\r\n--- STM32G4 Bootloader v1.0 ---\r\n";

  /* 1. Transmit initial startup banner over USART2 */
  HAL_UART_Transmit(&huart2, (uint8_t*)boot_banner, strlen(boot_banner), HAL_MAX_DELAY);

  /* 2. Read PC13 (Blue Button B1): Check if pressed on reset */
  if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET)
  {
      /* Button Pressed: Turn ON Green LED (PA5) and start UART listener */
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
      printmsg("[INFO] Blue Button Pressed -> Entering Custom Bootloader Mode...\r\n");

      bootloader_uart_read_data();
  }
  else
  {
      /* Button Released: Turn OFF Green LED and jump to app */
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
      printmsg("[INFO] Button Not Pressed -> Preparing Jump to Application...\r\n");

      bootloader_jump_to_user_app();
  }
  /* USER CODE END 2 */

  while (1)
  {
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_CRC_Init(void)
{
  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  /* UPDATED CONFIGURATION */
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;

  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /* PA5 - LD2 Green LED */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PC13 - Blue User Button */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

void printmsg(char *format, ...)
{
    char str[128];
    va_list args;

    va_start(args, format);
    vsnprintf(str, sizeof(str), format, args);
    va_end(args);

    HAL_UART_Transmit(D_UART, (uint8_t*)str, strlen(str), HAL_MAX_DELAY);
}

void bootloader_uart_read_data(void)
{
    while(1)
    {
        memset(bl_rx_buffer, 0, sizeof(bl_rx_buffer));

        /* Read Byte 0: Length To Follow */
        if (HAL_UART_Receive(&huart2, &bl_rx_buffer[0], 1, HAL_MAX_DELAY) == HAL_OK)
        {
            uint8_t len_to_follow = bl_rx_buffer[0];

            /* Read remaining packet bytes based on header count */
            if (HAL_UART_Receive(&huart2, &bl_rx_buffer[1], len_to_follow, HAL_MAX_DELAY) == HAL_OK)
            {
                bootloader_parse_and_execute(bl_rx_buffer);
            }
        }
    }
}

__weak void bootloader_parse_and_execute(uint8_t *pBuffer)
{
    /* Fallback parser if bootloader.c is not linked */
    printmsg("[BL] Command frame received.\r\n");
}

void bootloader_jump_to_user_app(void)
{
    uint32_t app_sp    = *(volatile uint32_t *)FLASH_APP_ADDR;
    uint32_t app_reset = *(volatile uint32_t *)(FLASH_APP_ADDR + 4U);

    /* Is there a valid app? SP in RAM, reset vector inside the app region */
    if (app_sp < 0x20000000U || app_sp > 0x20020000U ||
        app_reset < FLASH_APP_ADDR || app_reset >= 0x08080000U)
    {
        printmsg("[ERR] No valid application found\r\n");
        bootloader_uart_read_data();            /* stay in bootloader */
        return;
    }

    /* 1. De-init HAL while SysTick still works */
    HAL_UART_DeInit(&huart2);
    HAL_RCC_DeInit();
    HAL_DeInit();

    /* 2. Now silence everything */
    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    /* 3. Hand over: vector table, stack, then jump */
    SCB->VTOR = FLASH_APP_ADDR;
    __set_MSP(app_sp);
    __DSB();
    __ISB();
                                  /* PRIMASK survives a jump */

    ((pFunction)app_reset)();
    while (1) { }
}

/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
