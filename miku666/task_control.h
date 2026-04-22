#ifndef __TASK_CONTROL_H
#define __TASK_CONTROL_H

#include "cmsis_os.h"
#include "main.h"
#include "pid.h"
#include "fuzzy_pid.h"
#include "advanced_pid.h"

// 串口打印数据结构体
typedef struct {
    uint32_t timestamp;
    float current_temp;
    float pwm_out;
    float p_out;
    float i_out;
    float d_out;
} UARTPrintData_t;

extern QueueHandle_t UARTPrintQueueHandle;

extern FuzzyPID_struct fuzzy_pid_TEMP;
extern AdvPID_struct adv_pid_TEMP;

void StartTask_Control(void const * argument);

#endif
