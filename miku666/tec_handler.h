#ifndef TEC_HANDLER_H
#define TEC_HANDLER_H

#include <stdint.h>

// TEC 状态结构体
typedef struct {
    float input_voltage;     // 输入电压
    float set_temperature;   // 设定温度
    float real_temperature;  // 实时温度
    float tec_voltage;       // TEC电压
    float tec_current;       // TEC电流
    float pcb_temperature;   // PCB温度
    uint8_t heating_cooling_status; // 加热/制冷状态
    uint16_t alarm_code;     // 报警代码
} TEC_Status_t;

// 函数声明
void TEC_Init(void);
void TEC_Tick(void);
void TEC_PrintInfo(void);

#endif /* TEC_HANDLER_H */
