/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch9431_port.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/06/30
 * Description        : This file contains ch9431 port to ESP32.
 *                      移植自 WCH CH9431EVT 的示例代码（EVT\EXAM\CH32F103\PUB\ch9431_port.h），
 *                      原版面向 CH32，此处改为 ESP32 的延时/GPIO 引脚操作，
 *                      函数名保持 _port_ch9431_xxx 不变。
 * 移植者             : IT老大哥
 * 移植日期           : 2026-10-08
*********************************************************************************
* Copyright (c) 2023 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#ifndef _CH9431_PORT_H_
#define _CH9431_PORT_H_

#include <stdint.h>

/* 忙等延时，单位 us / ms（配置寄存器、复位等待都要留够时间） */
void _port_ch9431_delay_us(uint32_t t);
void _port_ch9431_delay_ms(uint32_t t);

/* RST 引脚（低有效） */
void _port_ch9431_rst_high(void);
void _port_ch9431_rst_low(void);

/* CS#：拉低开始一条命令，拉高结束；内部同时负责命令互斥与指令间隔 */
void _port_ch9431_spi_nss_high(void);
void _port_ch9431_spi_nss_low(void);

/* TXnRTS：用引脚方式请求发送时用，本组件默认走寄存器请求，未接时可不管 */
void _port_ch9431_tx0rts_low(void);
void _port_ch9431_tx0rts_high(void);
void _port_ch9431_tx1rts_low(void);
void _port_ch9431_tx1rts_high(void);
void _port_ch9431_tx2rts_low(void);
void _port_ch9431_tx2rts_high(void);

/* SPI 全双工交换一个字节（8bit、Mode 0），返回收到的字节 */
uint16_t _port_ch9431_spi_rw_data(uint16_t data);

#endif /* _CH9431_PORT_H_ */
