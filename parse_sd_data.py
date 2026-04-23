#!/usr/bin/env python3
"""
SD卡二进制数据解析脚本

功能：解析STM32保存的二进制数据文件，将数据转换为可读格式
数据格式：
- 每条记录20字节
- 字段：
  1. timestamp: uint32_t (4字节) - 时间戳(ms)
  2. temp: int16_t (2字节) - 温度 × 100
  3. pwm: int16_t (2字节) - PWM × 100
  4. p[0]: int16_t (2字节) - P系数(100ms) × 100
  5. p[1]: int16_t (2字节) - P系数(200ms) × 100
  6. i[0]: int16_t (2字节) - I系数(100ms) × 100
  7. i[1]: int16_t (2字节) - I系数(200ms) × 100
  8. d[0]: int16_t (2字节) - D系数(100ms) × 100
  9. d[1]: int16_t (2字节) - D系数(200ms) × 100
"""

import struct
import sys
import os

def parse_bin_file(file_path):
    """解析二进制数据文件"""
    if not os.path.exists(file_path):
        print(f"错误：文件不存在 - {file_path}")
        return
    
    try:
        with open(file_path, 'rb') as f:
            print(f"解析文件: {file_path}")
            print("=" * 80)
            print(f"{'时间戳(ms)':<12} {'温度(°C)':<10} {'PWM(%)':<8} {'P(100|200)':<12} {'I(100|200)':<12} {'D(100|200)':<12}")
            print("-" * 80)
            
            record_count = 0
            while True:
                # 读取20字节数据
                data = f.read(20)
                if len(data) < 20:
                    break
                
                # 解析数据（小端字节序）
                timestamp, temp, pwm, p0, p1, i0, i1, d0, d1 = struct.unpack('<IHHhhhhhh', data)
                
                # 转换为实际值
                temp_val = temp / 100.0
                pwm_val = pwm / 100.0
                p_100 = p0 / 100.0
                p_200 = p1 / 100.0
                i_100 = i0 / 100.0
                i_200 = i1 / 100.0
                d_100 = d0 / 100.0
                d_200 = d1 / 100.0
                
                # 打印数据
                print(f"{timestamp:<12} {temp_val:<10.2f} {pwm_val:<8.2f} {p_100:6.2f}|{p_200:6.2f} {i_100:6.2f}|{i_200:6.2f} {d_100:6.2f}|{d_200:6.2f}")
                
                record_count += 1
            
            print("-" * 80)
            print(f"总记录数: {record_count}")
            print("=" * 80)
            
    except Exception as e:
        print(f"解析错误: {e}")

def main():
    """主函数"""
    # 直接使用硬编码路径
    file_path = "data_17.bin"  # 或者使用完整路径
    parse_bin_file(file_path)

if __name__ == "__main__":
    main()