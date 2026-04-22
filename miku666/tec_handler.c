#include "tec_handler.h"
#include "stm32h7xx_hal.h"
#include "usart.h"
#include "../Core/Inc/main.h"
#include <string.h>

// 全局变量
extern UART_HandleTypeDef huart4;
TEC_Status_t tec_status;
uint8_t tec_tx_buffer[8] = {0x01, 0x04, 0x75, 0x30, 0x00, 0x08, 0xEB, 0xCF};
uint8_t tec_rx_buffer[21];
uint32_t tec_last_comm_time = 0;
uint8_t tec_comm_timeout = 0;

// CRC-16/MODBUS 计算函数
uint16_t CRC16_MODBUS(uint8_t *data, uint8_t length) {
    uint16_t crc = 0xFFFF;
    uint8_t i, j;
    
    for (i = 0; i < length; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// 初始化 TEC 模块
void TEC_Init(void) {
    // 这里需要根据实际硬件配置 UART4
    // 假设已经在其他地方初始化了 UART4
    // 波特率：115200，8位数据，1位停止位，无校验
    
    // 初始化状态结构体
    memset(&tec_status, 0, sizeof(TEC_Status_t));
    
    // 记录初始时间
    tec_last_comm_time = HAL_GetTick();
}

void TEC_Tick(void) {
    uint32_t current_time = HAL_GetTick();
    if (current_time - tec_last_comm_time > 1000) tec_comm_timeout = 1;

    // 1. 发送前先尝试清空接收缓冲区，防止“回显”干扰
    __HAL_UART_FLUSH_DRREGISTER(&huart4);

    // 2. 发送查询指令
    HAL_GPIO_WritePin(flag_485_GPIO_Port, flag_485_Pin, GPIO_PIN_SET);
    HAL_UART_Transmit(&huart4, tec_tx_buffer, 8, 100);
    
    // 重要：等待发送彻底完成再切模式
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET);
    HAL_GPIO_WritePin(flag_485_GPIO_Port, flag_485_Pin, GPIO_PIN_RESET);

    // 【修改点】：每次接收前先把全局数组清零，防止旧数据干扰视线
    memset(tec_rx_buffer, 0, 21);

    // 【修改点】：直接把数据接收到全局的 tec_rx_buffer 中
    if (HAL_UART_Receive(&huart4, tec_rx_buffer, 21, 150) == HAL_OK) {
        
        // 自动寻找合法的帧头 01 04 10
        uint8_t *p = NULL;
        for(int i=0; i<5; i++) {
            // 注意这里也改成了 tec_rx_buffer
            if(tec_rx_buffer[i] == 0x01 && tec_rx_buffer[i+1] == 0x04 && tec_rx_buffer[i+2] == 0x10) {
                p = &tec_rx_buffer[i];
                break;
            }
        }

        if (p != NULL) {
            // 解析物理量
            tec_status.input_voltage = ((uint16_t)p[3] << 8 | p[4]) / 100.0f;
            tec_status.set_temperature = (int16_t)((uint16_t)p[5] << 8 | p[6]) / 100.0f;
            tec_status.real_temperature = (int16_t)((uint16_t)p[7] << 8 | p[8]) / 100.0f;
            tec_status.tec_voltage = (int16_t)((uint16_t)p[9] << 8 | p[10]) / 100.0f;
            tec_status.tec_current = (int16_t)((uint16_t)p[11] << 8 | p[12]) / 1000.0f;
            tec_status.pcb_temperature = (int16_t)((uint16_t)p[13] << 8 | p[14]) / 100.0f;
            tec_status.heating_cooling_status = p[16];
            tec_status.alarm_code = (uint16_t)p[17] << 8 | p[18];

            tec_last_comm_time = current_time;
            tec_comm_timeout = 0;
        }
    }
}

// 打印 TEC 信息
void TEC_PrintInfo(void) {
    if (tec_comm_timeout) {
        printf("TEC Communication Timeout\r\n");
        printf("Raw Data: ");
        for (int i = 0; i < 21; i++) {
            printf("%02X ", tec_rx_buffer[i]);
        }
        printf("\r\n\r\n");
        return;
    }
    
    printf("TEC Status:\r\n");
    printf("Input Voltage: %.2fV\r\n", tec_status.input_voltage);
    printf("Set Temperature: %.2fC\r\n", tec_status.set_temperature);
    printf("Real Temperature: %.2fC\r\n", tec_status.real_temperature);
    printf("TEC Voltage: %.2fV\r\n", tec_status.tec_voltage);
    printf("TEC Current: %.3fA\r\n", tec_status.tec_current);
    printf("PCB Temperature: %.2fC\r\n", tec_status.pcb_temperature);
    printf("Heating/Cooling Status: %d\r\n", tec_status.heating_cooling_status);
    printf("Alarm Code: %d\r\n\r\n", tec_status.alarm_code);
}
