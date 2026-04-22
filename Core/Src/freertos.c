/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_touchgfx.h"
#include "temp_filter.h"
#include "alu_temp.h"
#include "alu_control.h"
#include "alu_file.h"
#include "task_control.h"
#include "alu_main.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* 定义一个全局的 SD 卡写入队列句柄 */
QueueHandle_t SDWriteQueueHandle = NULL;

/* 定义一个全局的串口打印队列句柄 */
QueueHandle_t UARTPrintQueueHandle = NULL;

/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId aluMainHandle;
osThreadId aluSubProgressHandle;
osThreadId Task_ControlHandle;
osMessageQId alu_temp_listHandle;
osSemaphoreId alu_temperatureHandle;
osSemaphoreId alu_thresholdHandle;
osSemaphoreId alu_screenHandle;
osSemaphoreId alu_chooseHandle;
osSemaphoreId alu_savenameHandle;
osSemaphoreId Sem_10msHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartTouchGFX(void const * argument);
void AluMain(void const * argument);
void AluSubProgress(void const * argument);
void StartTask_Control(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* GetTimerTaskMemory prototype (linked to static allocation support) */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/* USER CODE BEGIN GET_TIMER_TASK_MEMORY */
static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
  *ppxTimerTaskStackBuffer = &xTimerStack[0];
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
  /* place for user code */
}
/* USER CODE END GET_TIMER_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of alu_temperature */
  osSemaphoreDef(alu_temperature);
  alu_temperatureHandle = osSemaphoreCreate(osSemaphore(alu_temperature), 1);

  /* definition and creation of alu_threshold */
  osSemaphoreDef(alu_threshold);
  alu_thresholdHandle = osSemaphoreCreate(osSemaphore(alu_threshold), 1);

  /* definition and creation of alu_screen */
  osSemaphoreDef(alu_screen);
  alu_screenHandle = osSemaphoreCreate(osSemaphore(alu_screen), 1);

  /* definition and creation of alu_choose */
  osSemaphoreDef(alu_choose);
  alu_chooseHandle = osSemaphoreCreate(osSemaphore(alu_choose), 1);

  /* definition and creation of alu_savename */
  osSemaphoreDef(alu_savename);
  alu_savenameHandle = osSemaphoreCreate(osSemaphore(alu_savename), 1);

  /* definition and creation of Sem_10ms */
  osSemaphoreDef(Sem_10ms);
  Sem_10msHandle = osSemaphoreCreate(osSemaphore(Sem_10ms), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of alu_temp_list */
  osMessageQDef(alu_temp_list, 100, uint32_t);
  alu_temp_listHandle = osMessageCreate(osMessageQ(alu_temp_list), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* 串口打印队列，长度40 */
  osMessageQDef(uart_print_queue, 40, UARTPrintData_t);
  UARTPrintQueueHandle = osMessageCreate(osMessageQ(uart_print_queue), NULL);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartTouchGFX, osPriorityNormal, 0, 1536);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of aluMain */
  osThreadDef(aluMain, AluMain, osPriorityNormal, 0, 1536);
  aluMainHandle = osThreadCreate(osThread(aluMain), NULL);

  /* definition and creation of aluSubProgress */
  osThreadDef(aluSubProgress, AluSubProgress, osPriorityAboveNormal, 0, 1536);
  aluSubProgressHandle = osThreadCreate(osThread(aluSubProgress), NULL);

  /* definition and creation of Task_Control */
  osThreadDef(Task_Control, StartTask_Control, osPriorityRealtime, 0, 1024);
  Task_ControlHandle = osThreadCreate(osThread(Task_Control), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartTouchGFX */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartTouchGFX */
__weak void StartTouchGFX(void const * argument)
{
  /* USER CODE BEGIN StartTouchGFX */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTouchGFX */
}

/* USER CODE BEGIN Header_AluMain */
/**
* @brief Function implementing the aluMain thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AluMain */
__weak void AluMain(void const * argument)
{
  /* USER CODE BEGIN AluMain */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END AluMain */
}

/* USER CODE BEGIN Header_AluSubProgress */
/**
* @brief Function implementing the aluSubProgress thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_AluSubProgress */
__weak void AluSubProgress(void const * argument)
{
  /* USER CODE BEGIN AluSubProgress */
  UARTPrintData_t rx_data;
  UARTPrintData_t print_buf[20];
  uint8_t buf_count = 0;
  uint32_t tick_count = 0;
  
  // 打印耗时统计变量
  uint32_t print_time_max = 0;
  uint32_t print_time_sum = 0;
  uint32_t print_time_cnt = 0;

  // 启用DWT循环计数器
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  for(;;)
  {
      tick_count++;
      
      // 等待队列数据，超时时间设为 10ms
      if (xQueueReceive(UARTPrintQueueHandle, &rx_data, 10) == pdPASS) {
          print_buf[buf_count++] = rx_data;
          
          // 如果攒够了20个数据（相当于200ms），一次性批量打印
          if (buf_count >= 20) {
              uint32_t start_time = DWT->CYCCNT;
              
              for (int i = 0; i < buf_count; i++) {
                  printf("[%lu]%.2f,%.3g,%.3g,%.2g,%.1f\r\n", 
                         print_buf[i].timestamp, 
                         print_buf[i].current_temp, 
                         print_buf[i].p_out, 
                         print_buf[i].i_out, 
                         print_buf[i].d_out,
                         print_buf[i].pwm_out);
              }
              
              uint32_t end_time = DWT->CYCCNT;
              uint32_t elapsed = (end_time >= start_time) ? (end_time - start_time) : (0xFFFFFFFF - start_time + end_time);
              print_time_sum += elapsed;
              print_time_cnt++;
              if (elapsed > print_time_max) {
                  print_time_max = elapsed;
              }
              
              buf_count = 0;
          }
      } else {
          // 如果10ms内没收到新数据，说明可能停止加热了。把缓存里剩下的数据强制打印出来
          if (buf_count > 0) {
              uint32_t start_time = DWT->CYCCNT;
              
              for (int i = 0; i < buf_count; i++) {
                  printf("[%lu]%.2f,%.3g,%.3g,%.2g,%.1f\r\n", 
                         print_buf[i].timestamp, 
                         print_buf[i].current_temp, 
                         print_buf[i].p_out, 
                         print_buf[i].i_out, 
                         print_buf[i].d_out,
                         print_buf[i].pwm_out);
              }
              
              uint32_t end_time = DWT->CYCCNT;
              uint32_t elapsed = (end_time >= start_time) ? (end_time - start_time) : (0xFFFFFFFF - start_time + end_time);
              print_time_sum += elapsed;
              print_time_cnt++;
              if (elapsed > print_time_max) {
                  print_time_max = elapsed;
              }
              
              buf_count = 0;
          }
      }
      
      // 每100个循环（约1秒）打印一次耗时统计
      if (tick_count % 100 == 0) {
          if (enable_print_timing == 1 && print_time_cnt > 0) {
              float avg_us = (float)print_time_sum / print_time_cnt / (SystemCoreClock / 1000000.0f);
              float max_us = (float)print_time_max / (SystemCoreClock / 1000000.0f);
              printf("\r\n\r\n===== PRINT TIMING (1s) =====\r\n");
              printf("Max: %.1fus(%.0fcycles), Avg: %.1fus(%.0fcycles), Count: %u\r\n",
                     max_us, (float)print_time_max, avg_us, (float)print_time_sum / print_time_cnt, (unsigned int)print_time_cnt);
              printf("=============================\r\n\r\n");
          }

          // 每1秒打印一次堆栈信息，无论是否在加热状态
          if (enable_stack_print == 1) {
              printf("[STACK] aluMain:%u, defaultTask:%u, Control:%u, SubProg:%u\r\n",
                     (unsigned int)uxTaskGetStackHighWaterMark(aluMainHandle),
					 (unsigned int)uxTaskGetStackHighWaterMark(defaultTaskHandle),
                     (unsigned int)uxTaskGetStackHighWaterMark(Task_ControlHandle),
                     (unsigned int)uxTaskGetStackHighWaterMark(aluSubProgressHandle));
          }
          
          // 重置统计变量
          print_time_max = 0;
          print_time_sum = 0;
          print_time_cnt = 0;
      }
  }
  /* USER CODE END AluSubProgress */
}

/* USER CODE BEGIN Header_StartTask_Control */
/**
* @brief Function implementing the Task_Control thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask_Control */
__weak void StartTask_Control(void const * argument)
{
  /* USER CODE BEGIN StartTask_Control */
  /* Infinite loop */
for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTask_Control */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
