#include "task_control.h"
#include "temp_filter.h"
#include "pid.h"
#include "fuzzy_pid.h"
#include "advanced_pid.h"
#include "thermocouple.h"
#include "alu_control.h"
#include "alu_file.h"
#include "alu_temp.h"
#include "alu_main.h"
#include "dac.h"
#include "core_cm7.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"

extern UART_HandleTypeDef huart4;
extern TIM_HandleTypeDef htim1;

extern QueueHandle_t SDWriteQueueHandle;
extern QueueHandle_t UARTPrintQueueHandle;
extern osSemaphoreId Sem_10msHandle;
extern osSemaphoreId alu_temperatureHandle;
extern osSemaphoreId alu_screenHandle;

extern osThreadId defaultTaskHandle;
extern osThreadId aluMainHandle;
extern osThreadId aluSubProgressHandle;
extern osThreadId Task_ControlHandle;

extern double K_Temperature;
extern float temp_modify;
extern float temp_thres;
extern float pwm_percent;

extern volatile uint8_t is_heating_active;
extern volatile uint32_t heating_num_count;
extern uint8_t sd_record_enable;
extern PID_struct pid_TEMP;
extern uint8_t uart_pid_state;

extern uint8_t need_stop_cleanup;
extern uint8_t enable_stack_print;
extern uint8_t enable_print_timing;

extern FuzzyPID_struct fuzzy_pid_TEMP;
extern AdvPID_struct adv_pid_TEMP;

static uint8_t last_heating_active = 0;

void StartTask_Control(void const * argument)
{
    // ================================================
    // 时间基准: 10ms (TIM7定时器中断触发)
    // 计数器说明:
    //   tick_10ms % 20 == 0  → 200ms间隔 (SD卡写入/温度打印)
    //   tick_10ms % 100 == 0 → 1000ms间隔 (堆栈打印)
    // ================================================

    TempFilter_Init();
    FuzzyPID_init(&fuzzy_pid_TEMP);
    AdvPID_Init(&adv_pid_TEMP, 40.0f, 0.8f, 125.0f);
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    uint32_t tick_10ms = 0;
    uint32_t heating_startup_counter = 0;
    uint8_t alu_485_off[] = {0x55, 0x33, 0x01, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x0D};

    #define TRIM_SAMPLES 20
    #define TRIM_REMOVE 4

    float temp_buffer[TRIM_SAMPLES] = {0};
    uint8_t temp_buffer_index = 0;
    float temp_avg_200ms = 0.0f;

    float pwm_buffer[TRIM_SAMPLES] = {0};
    uint8_t pwm_buffer_index = 0;
    float pwm_avg_200ms = 0.0f;

    float temp_sum_1s = 0.0f;  // 1s温度累加器
    uint32_t temp_count_1s = 0;  // 1s采样计数
    float temp_avg_1s = 0.0f;  // 1s平均温度



    for(;;)
    {
        osSemaphoreWait(Sem_10msHandle, osWaitForever);  // 等待10ms定时器信号
        tick_10ms++;

        // ================================================
        // 10ms每次: 加热打印(T,P,I,D) + 计时 + 温度滤波采集 + 累加平均 + 脚踏检测 + PID控制输出
        // ================================================
        if (is_heating_active == 1) {
            float p_out = 0.0f, i_out = 0.0f, d_out = 0.0f;
            if (pid_algorithm_type == 0) {
                p_out = pid_TEMP.speed[0];
                i_out = pid_TEMP.speed[1];
                d_out = pid_TEMP.speed[2];
            } else if (pid_algorithm_type == 1) {
                p_out = fuzzy_pid_TEMP.speed[0];
                i_out = fuzzy_pid_TEMP.speed[1];
                d_out = fuzzy_pid_TEMP.speed[2];
            } else {
                p_out = adv_pid_TEMP.speed[0];
                i_out = adv_pid_TEMP.speed[1];
                d_out = adv_pid_TEMP.speed[2];
            }

            // 打包数据并发送到打印队列（0阻塞，满了直接丢弃防卡死）
            if (UARTPrintQueueHandle != NULL) {
                UARTPrintData_t print_data;
                print_data.timestamp = tick_10ms * 10;
                print_data.current_temp = K_Temperature;
                print_data.pwm_out = pwm_percent * 100.0f;
                print_data.p_out = p_out;
                print_data.i_out = i_out;
                print_data.d_out = d_out;
                xQueueSend(UARTPrintQueueHandle, &print_data, 0);
            }
        }

        TempFilter_Process();
        K_Temperature = TempFilter_GetUIAvgTemp();

        temp_buffer[temp_buffer_index] = K_Temperature;
        temp_buffer_index = (temp_buffer_index + 1) % TRIM_SAMPLES;
        temp_sum_1s += K_Temperature;
        temp_count_1s++;

        if (last_heating_active == 0 && is_heating_active == 1) {
            heating_startup_counter = 0;
            adv_pid_TEMP.last_measured = (float)K_Temperature;
            adv_pid_TEMP.last_d_out = 0.0f;
            adv_pid_TEMP.heating_time_samples = 0;
        }
        last_heating_active = is_heating_active;

        if (is_heating_active == 1) {
            heating_startup_counter++;
        }

        // ================================================
        // 脚踏抬起检测: 停止加热 (需踩下超过30ms有效)
        // ================================================
        if (HAL_GPIO_ReadPin(btn_foot_GPIO_Port, btn_foot_Pin) == 0 && is_heating_active == 1) {
            if (heating_startup_counter > 3) {
                is_heating_active = 0;

                HAL_GPIO_WritePin(flag_485_GPIO_Port, flag_485_Pin, GPIO_PIN_SET);
                HAL_UART_Transmit(&huart4, alu_485_off, 10, 100);
                HAL_GPIO_WritePin(flag_485_GPIO_Port, flag_485_Pin, GPIO_PIN_RESET);

                __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 1);
                DAC_SetLaserCurrent(0.0f);
                need_stop_cleanup = 1;
            }
        }

        // ================================================
        // PID控制: 温度采集 + 闭环计算 + PWM输出
        // ================================================
        if (is_heating_active == 1) {
            K_Temperature = K_Temperature + temp_modify;
            if (K_Temperature >= 150) K_Temperature = 150;
            else if (K_Temperature <= 0) K_Temperature = 0;

            float pwm_value = 0.0f;

            if (laser_test_state == 3) {
                extern float target_laser_current;
                extern float target_laser_pwm;
                DAC_SetLaserCurrent(target_laser_current);
                pwm_value = target_laser_pwm;
            } else {
                if (pid_algorithm_type == 0) {
                    pwm_value = PID_PWM_iteration(&pid_TEMP, temp_thres, K_Temperature);
                } else if (pid_algorithm_type == 1) {
                    pwm_value = FuzzyPID_Calculate(&fuzzy_pid_TEMP, temp_thres, K_Temperature);
                } else {
                    pwm_value = AdvPID_Calculate(&adv_pid_TEMP, temp_thres, (float)K_Temperature);
                }
            }
            
            // 限制PWM值范围
            if (pwm_value < 0) pwm_value = 0;
            if (pwm_value > 1000) pwm_value = 1000;
            
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, (uint32_t)pwm_value);
            pwm_percent = pwm_value / 1000.0f;
            
            pwm_buffer[pwm_buffer_index] = pwm_percent;
            pwm_buffer_index = (pwm_buffer_index + 1) % TRIM_SAMPLES;
        } else {
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 1);
        }

        // ================================================
        // 200ms间隔: 冷端补偿更新 + SD卡数据写入 + 温度打印(非加热时)
        // ================================================
        if (tick_10ms % 20 == 0) {
            Thermocouple_UpdateColdJunction();

            float temp_sorted[TRIM_SAMPLES];
            float pwm_sorted[TRIM_SAMPLES];
            
            for (int i = 0; i < TRIM_SAMPLES; i++) {
                temp_sorted[i] = temp_buffer[i];
                pwm_sorted[i] = pwm_buffer[i];
            }
            
            for (int i = 0; i < TRIM_SAMPLES - 1; i++) {
                for (int j = 0; j < TRIM_SAMPLES - i - 1; j++) {
                    if (temp_sorted[j] > temp_sorted[j + 1]) {
                        float temp = temp_sorted[j];
                        temp_sorted[j] = temp_sorted[j + 1];
                        temp_sorted[j + 1] = temp;
                    }
                }
            }
            
            for (int i = 0; i < TRIM_SAMPLES - 1; i++) {
                for (int j = 0; j < TRIM_SAMPLES - i - 1; j++) {
                    if (pwm_sorted[j] > pwm_sorted[j + 1]) {
                        float temp = pwm_sorted[j];
                        pwm_sorted[j] = pwm_sorted[j + 1];
                        pwm_sorted[j + 1] = temp;
                    }
                }
            }
            
            float temp_sum = 0.0f;
            for (int i = TRIM_REMOVE; i < TRIM_SAMPLES - TRIM_REMOVE; i++) {
                temp_sum += temp_sorted[i];
            }
            temp_avg_200ms = temp_sum / (TRIM_SAMPLES - 2 * TRIM_REMOVE);
            
            float pwm_sum = 0.0f;
            for (int i = TRIM_REMOVE; i < TRIM_SAMPLES - TRIM_REMOVE; i++) {
                pwm_sum += pwm_sorted[i];
            }
            pwm_avg_200ms = pwm_sum / (TRIM_SAMPLES - 2 * TRIM_REMOVE);

            if (is_heating_active == 0 && is_serial_interacting == 0 && uart_pid_state == 0) {
                printf("%.2f(off)\r\n", temp_avg_200ms);  // 非加热时200ms平均打印一次
            }

            if (is_heating_active == 1) {
                char BufferWrite[64] = {0};
                float sd_p_out, sd_i_out, sd_d_out;
                if (pid_algorithm_type == 0) {
                    sd_p_out = pid_TEMP.speed[0];
                    sd_i_out = pid_TEMP.speed[1];
                    sd_d_out = pid_TEMP.speed[2];
                } else if (pid_algorithm_type == 1) {
                    sd_p_out = fuzzy_pid_TEMP.speed[0];
                    sd_i_out = fuzzy_pid_TEMP.speed[1];
                    sd_d_out = fuzzy_pid_TEMP.speed[2];
                } else {
                    sd_p_out = adv_pid_TEMP.speed[0];
                    sd_i_out = adv_pid_TEMP.speed[1];
                    sd_d_out = adv_pid_TEMP.speed[2];
                }
                sprintf(BufferWrite, "\n%lu,%.2f,%.3g,%.3g,%.2g,%.1f",
                    tick_10ms * 10, temp_avg_200ms,
                    sd_p_out, sd_i_out, sd_d_out,
                    pwm_avg_200ms * 100.0f);

                if (sd_record_enable && SDWriteQueueHandle != NULL) {
                    xQueueSend(SDWriteQueueHandle, BufferWrite, 0);
                }
            }
        }

        // ================================================
        // 1000ms间隔: 计算1s平均温度 + 信号量释放 + 堆栈打印 + 打印最长耗时统计
        // ================================================
        if (tick_10ms % 100 == 0) {
            if (temp_count_1s > 0) {
                temp_avg_1s = temp_sum_1s / temp_count_1s;
            }
            temp_sum_1s = 0.0f;
            temp_count_1s = 0;

            extern float g_display_temp_avg;
            g_display_temp_avg = temp_avg_1s;
            osSemaphoreRelease(alu_temperatureHandle);

            if (enable_stack_print == 1) {
                printf("[STACK] aluMain:%u, Control:%u, SubProg:%u\r\n",
                       (unsigned int)uxTaskGetStackHighWaterMark(aluMainHandle),
                       (unsigned int)uxTaskGetStackHighWaterMark(Task_ControlHandle),
                       (unsigned int)uxTaskGetStackHighWaterMark(aluSubProgressHandle));
            }


        }
    }
}
