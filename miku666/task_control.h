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

// SD卡二进制数据结构体 (26字节)
#pragma pack(1)
typedef struct {
    uint32_t timestamp;   // 时间戳
    int16_t temp;         // 温度 × 100
    int16_t pwm;          // PWM × 100
    int16_t p[2];         // P分量 [0]=100ms值 [1]=200ms值，各×100
    int16_t i[2];         // I分量 [0]=100ms值 [1]=200ms值，各×100
    int16_t d[2];         // D分量 [0]=100ms值 [1]=200ms值，各×100
} SD_DataPacket_t;
#pragma pack()

extern QueueHandle_t UARTPrintQueueHandle;

extern FuzzyPID_struct fuzzy_pid_TEMP;
extern AdvPID_struct adv_pid_TEMP;

void StartTask_Control(void const * argument);

#endif
