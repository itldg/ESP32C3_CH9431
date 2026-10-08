/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch9431_cmd.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/10/10
 * Description        : This file contains ch9431 command example.
 *                      移植自 WCH CH9431EVT 的示例代码（EVT\EXAM\CH32F103\PUB\ch9431_cmd.c），
 *                      逻辑保持不变，仅把 FunctionalState 换成 ch9431_enable_t、
 *                      函数名统一成 ch9431_snake_case。
 * 移植者             : IT老大哥
 * 移植日期           : 2026-10-08
*********************************************************************************
* Copyright (c) 2023 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#include "ch9431.h"
#include "ch9431_port.h"

/*********************************************************************
 * @fn      ch9431_write_byte
 *
 * @brief   向 CH9431 的寄存器写一个字节（写命令 + 地址 + 数据，CS 全程拉低）。
 *
 * @param   addr  - 寄存器地址，见 ch9431_regs.h 里的 R8_xxx。
 * @param   value - 要写入的值。
 *
 * @return  none.
 */
void ch9431_write_byte(uint8_t addr, uint8_t value)
{
    _port_ch9431_spi_nss_low();                         /* 置CH9431的CS为低电平 */

    _port_ch9431_spi_rw_data(CMD_CAN_WRITE);            /* 发送写命令 */

    _port_ch9431_spi_rw_data(addr);                     /* 发送地址 */

    _port_ch9431_spi_rw_data(value);                    /* 写入数据 */

    _port_ch9431_spi_nss_high();                        /* 置CH9431的CS为高电平 */
}

/*********************************************************************
 * @fn      ch9431_read_byte
 *
 * @brief   读 CH9431 寄存器的一个字节（读命令 + 地址，等 CH9431_WAIT_DATA_US
 *          让芯片准备数据后再读出）。
 *
 * @param   addr - 寄存器地址。
 *
 * @return  该寄存器的值。
 */
uint8_t ch9431_read_byte(uint8_t addr)
{
    uint8_t rByte;

    _port_ch9431_spi_nss_low();                         /* 置CH9431的CS为低电平 */

    _port_ch9431_spi_rw_data(CMD_CAN_READ);             /* 发送读命令 */

    _port_ch9431_spi_rw_data(addr);                     /* 发送地址 */

    _port_ch9431_delay_us(CH9431_WAIT_DATA_US);         /* 等待CH9431处理，准备数据 */

    rByte = (uint8_t)_port_ch9431_spi_rw_data(0xff);    /* 读取数据 */

    _port_ch9431_spi_nss_high();                        /* 置CH9431的CS为高电平 */

    return rByte;
}

/*********************************************************************
 * @fn      ch9431_bit_modify
 *
 * @brief   按位修改寄存器：mask 里为 1 的位才用 value 改写，其余位保持原值。
 *          改标志位时用它，避免读-改-写期间丢事件。
 *
 * @param   addr  - 寄存器地址。
 * @param   mask  - 位屏蔽，1 表示该位要改。
 * @param   value - 这些位的新值。
 *
 * @return  none.
 */
void ch9431_bit_modify(uint8_t addr, uint8_t mask, uint8_t value)
{
    _port_ch9431_spi_nss_low();                         /* 置CH9431的CS为低电平 */

    _port_ch9431_spi_rw_data(CMD_CAN_BIT_MODIFY);       /* 发送BIT_MODIFY命令 */
    _port_ch9431_spi_rw_data(addr);                     /* 发送BIT_MODIFY地址 */
    _port_ch9431_spi_rw_data(mask);                     /* 发送BIT_MODIFY屏蔽字节 */
    _port_ch9431_spi_rw_data(value);                    /* 发送BIT_MODIFY修正字节 */

    _port_ch9431_spi_nss_high();                        /* 置CH9431的CS为高电平 */
}

/*********************************************************************
 * @fn      ch9431_read_status
 *
 * @brief   用 RD_STATUS 命令快速读状态字（TXB0/1/2 是否空闲、RXB0/RXB1
 *          是否有报文），一次 SPI 事务就能拿到，比逐个读寄存器快。
 *
 * @param   rd_status - 输出，rd_status_t 指针。
 *
 * @return  None.
 */
void ch9431_read_status(rd_status_t *rd_status)
{
    _port_ch9431_spi_nss_low();                         /* 置CH9431的CS为低电平 */

    _port_ch9431_spi_rw_data(CMD_CAN_RD_STATUS);        /* 发送读状态命令 */

    _port_ch9431_delay_us(CH9431_WAIT_DATA_US);         /* 等待CH9431处理，准备数据 */

    rd_status->value = (uint8_t)_port_ch9431_spi_rw_data(0xff);  /* 读取数据 */

    _port_ch9431_spi_nss_high();                        /* 置CH9431的CS为高电平 */
}

/*********************************************************************
 * @fn      ch9431_read_rx_status
 *
 * @brief   用 RX_STATUS 命令快速读接收状态，RX0I/RX1I 表示哪个接收缓冲器
 *          收到了报文（还有报文类型、滤波器命中信息）。
 *
 * @param   rx_status - 输出，rx_status_t 指针。
 *
 * @return  None.
 */
void ch9431_read_rx_status(rx_status_t *rx_status)
{
    _port_ch9431_spi_nss_low();                         /* 置CH9431的CS为低电平 */

    _port_ch9431_spi_rw_data(CMD_CAN_RX_STATUS);        /* 发送读RX状态命令 */

    _port_ch9431_delay_us(CH9431_WAIT_DATA_US);         /* 等待CH9431处理，准备数据 */

    rx_status->value = (uint8_t)_port_ch9431_spi_rw_data(0xff);  /* 读取数据 */

    _port_ch9431_spi_nss_high();                        /* 置CH9431的CS为高电平 */
}

/*********************************************************************
 * @fn      ch9431_rts
 *
 * @brief   请求发送：参数传 CH9431_ENABLE 时，对应 TXBnCTRL.TXREQ 置 1,
 *          芯片立即把该缓冲器里的报文发出去。
 *
 * @param   txb0 - CH9431_ENABLE 则请求发送 TXB0。
 * @param   txb1 - CH9431_ENABLE 则请求发送 TXB1。
 * @param   txb2 - CH9431_ENABLE 则请求发送 TXB2。
 *
 * @return  None.
 */
void ch9431_rts(ch9431_enable_t txb0, ch9431_enable_t txb1, ch9431_enable_t txb2)
{
    uint8_t cmd = CMD_CAN_RTS;

    _port_ch9431_spi_nss_low();                     /* 置CH9431的CS为低电平 */

    if (txb0 != CH9431_DISABLE)
    {
        cmd = cmd | 0x01;
    }
    if (txb1 != CH9431_DISABLE)
    {
        cmd = cmd | 0x02;
    }
    if (txb2 != CH9431_DISABLE)
    {
        cmd = cmd | 0x04;
    }

    _port_ch9431_spi_rw_data(cmd);                  /* 发送RTS命令 */

    _port_ch9431_spi_nss_high();                    /* 置CH9431的CS为高电平 */

    _port_ch9431_delay_us(CH9431_RTS_BUSY_US);
}

/*********************************************************************
 * @fn      ch9431_load_tx_id
 *
 * @brief   用 LOAD_TX 命令把 ID + 控制字（共 5 字节）一次写进 TXBn,
 *          比逐寄存器写快。
 *
 * @param   opt   - 选哪个发送缓冲器，ch9431_load_tx_id_t。
 * @param   tx_id - 要写入的 ID/控制字（load_tx_id_t）。
 *
 * @return  None.
 */
void ch9431_load_tx_id(ch9431_load_tx_id_t opt, load_tx_id_t *tx_id)
{
    uint8_t *data = tx_id->value;
    uint8_t len = 5;

    _port_ch9431_spi_nss_low();                             /* 置CH9431的CS为低电平 */
    _port_ch9431_spi_rw_data(CMD_CAN_LOAD_TX | opt);        /* 发送load TX命令 */

    while (len != 0)
    {
        _port_ch9431_spi_rw_data(*data);                    /* 写入数据 */
        len--;
        data++;
    }

    _port_ch9431_spi_nss_high();                            /* 置CH9431的CS为高电平 */
}

/*********************************************************************
 * @fn      ch9431_load_tx_data
 *
 * @brief   用 LOAD_TX 命令把 8 字节数据一次写进 TXBn。
 *
 * @param   opt     - 选哪个发送缓冲器，ch9431_load_tx_data_t。
 * @param   tx_data - 要写入的 8 字节数据（load_tx_data_t）。
 *
 * @return  None.
 */
void ch9431_load_tx_data(ch9431_load_tx_data_t opt, load_tx_data_t *tx_data)
{
    uint8_t *data = tx_data->TXBnD;
    uint8_t len = 8;

    _port_ch9431_spi_nss_low();                             /* 置CH9431的CS为低电平 */
    _port_ch9431_spi_rw_data(CMD_CAN_LOAD_TX | opt);        /* 发送load TX命令 */

    while (len != 0)
    {
        _port_ch9431_spi_rw_data(*data);                    /* 写入数据 */
        len--;
        data++;
    }

    _port_ch9431_spi_nss_high();                            /* 置CH9431的CS为高电平 */
}

/*********************************************************************
 * @fn      ch9431_read_rx_id
 *
 * @brief   用 RD_RX_BUFF 命令快速读出 RXBn 的 ID + DLC（共 5 字节）。
 *
 * @param   opt        - 选哪个接收缓冲器，ch9431_read_rx_id_t。
 * @param   read_rx_id - 输出，ID 与 DLC/RTR。
 *
 * @return  None.
 */
void ch9431_read_rx_id(ch9431_read_rx_id_t opt, read_rx_id_t *read_rx_id)
{
    uint8_t *data = read_rx_id->value;
    uint8_t len = 5;

    _port_ch9431_spi_nss_low();                                 /* 置CH9431的CS为低电平 */

    _port_ch9431_spi_rw_data(CMD_CAN_RD_RX_BUFF | opt);         /* 发送读RX缓冲命令 */

    _port_ch9431_delay_us(CH9431_WAIT_DATA_US);                 /* 等待CH9431处理，准备数据 */

    while (len)
    {
        *data = (uint8_t)_port_ch9431_spi_rw_data(0xff);        /* 读取数据 */
        data++;
        len--;
    }

    _port_ch9431_spi_nss_high();                                /* 置CH9431的CS为高电平 */
}

/*********************************************************************
 * @fn      ch9431_read_rx_data
 *
 * @brief   用 RD_RX_BUFF 命令快速读出 RXBn 的 8 字节数据。
 *
 * @param   opt          - 选哪个接收缓冲器，ch9431_read_rx_data_t。
 * @param   read_rx_data - 输出，8 字节数据。
 *
 * @return  None.
 */
void ch9431_read_rx_data(ch9431_read_rx_data_t opt, read_rx_data_t *read_rx_data)
{
    uint8_t *data = read_rx_data->RXBnD;
    uint8_t len = 8;

    _port_ch9431_spi_nss_low();                                 /* 置CH9431的CS为低电平 */

    _port_ch9431_spi_rw_data(CMD_CAN_RD_RX_BUFF | opt);         /* 发送读RX缓冲命令 */

    _port_ch9431_delay_us(CH9431_WAIT_DATA_US);                 /* 等待CH9431处理，准备数据 */

    while (len)
    {
        *data = (uint8_t)_port_ch9431_spi_rw_data(0xff);        /* 读取数据 */
        data++;
        len--;
    }

    _port_ch9431_spi_nss_high();                                /* 置CH9431的CS为高电平 */
}

/*********************************************************************
 * @fn      ch9431_reset
 *
 * @brief   软件复位（RESET 命令），并等 CH9431_START_READY_MS 让芯片启动。
 *          复位后芯片处于配置模式，可以直接写波特率/滤波器等配置寄存器。
 *
 * @param   none.
 *
 * @return  none.
 */
void ch9431_reset(void)
{
    _port_ch9431_spi_nss_low();                         /* 置CH9431的CS为低电平 */
    _port_ch9431_spi_rw_data(CMD_CAN_RESET);            /* 发送软件复位命令 */
    _port_ch9431_spi_nss_high();                        /* 置CH9431的CS为高电平 */
    _port_ch9431_delay_ms(CH9431_START_READY_MS);       /* 复位后，需要延迟等待系统启动 */
}

/*********************************************************************
 * @fn      ch9431_hard_reset
 *
 * @brief   拉 RST 引脚硬复位（低电平时间需按 RST 引脚上的电容调整）。
 *          复位后芯片同样回到配置模式。
 *
 * @param   none.
 *
 * @return  none.
 */
void ch9431_hard_reset(void)
{
    _port_ch9431_rst_low();
    _port_ch9431_delay_ms(1);                           /* 该时间需要根据rst引脚上的电容调整 */
    _port_ch9431_rst_high();
    _port_ch9431_delay_ms(CH9431_START_READY_MS);       /* 复位后，需要延迟等待系统启动 */
}

/*********************************************************************
 * @fn      ch9431_cs_wakeup
 *
 * @brief   拉低 CS 一小段时间把芯片从浅睡眠唤醒，再等 CH9431_WAKEUP_READY_MS 启动。
 *
 * @param   none.
 *
 * @return  none.
 */
void ch9431_cs_wakeup(void)
{
    _port_ch9431_spi_nss_low();
    _port_ch9431_delay_us(CH9431_CS_WAKEUP_US);
    _port_ch9431_spi_nss_high();
    _port_ch9431_delay_ms(CH9431_WAKEUP_READY_MS);      /* 等待系统启动 */
}

/*********************************************************************
 * @fn      ch9431_get_ver_string
 *
 * @brief   读 8 字节版本字符串（WCH 私有命令序列 0x00 + 0x76）。
 *          正常是 7 个 ASCII 字符加一个 '\0'；读回全 0 说明 MISO 上没有数据。
 *
 * @param   ver_str - 输出缓冲区，至少 8 字节。
 *
 * @return  none.
 */
void ch9431_get_ver_string(uint8_t *ver_str)
{
    _port_ch9431_spi_nss_low();                         /* 置CH9431的CS为低电平 */

    _port_ch9431_spi_rw_data(0);
    _port_ch9431_spi_rw_data(0x76);
    _port_ch9431_delay_us(CH9431_WAIT_DATA_US);         /* 等待CH9431处理，准备数据 */

    ver_str[0] = (uint8_t)_port_ch9431_spi_rw_data(0xff);
    ver_str[1] = (uint8_t)_port_ch9431_spi_rw_data(0xff);
    ver_str[2] = (uint8_t)_port_ch9431_spi_rw_data(0xff);
    ver_str[3] = (uint8_t)_port_ch9431_spi_rw_data(0xff);

    ver_str[4] = (uint8_t)_port_ch9431_spi_rw_data(0xff);
    ver_str[5] = (uint8_t)_port_ch9431_spi_rw_data(0xff);
    ver_str[6] = (uint8_t)_port_ch9431_spi_rw_data(0xff);
    ver_str[7] = (uint8_t)_port_ch9431_spi_rw_data(0xff);

    _port_ch9431_spi_nss_high();                        /* 置CH9431的CS为高电平 */
}

/*********************************************************************
 * @fn      ch9431_enter_iap
 *
 * @brief   进入 IAP 固件升级模式：先发随机数和芯片握手，再用读回的校验值
 *          发第二次认证，最后确认 SYSSTAT.OPMOD 已切到 IAP。
 *
 * @param   rand - 应用侧生成的随机数，用于和芯片握手。
 *
 * @return  0 - 进入成功.
 *          1 - 进入失败.
 */
uint8_t ch9431_enter_iap(uint32_t rand)
{
    uint8_t read_data[4];
    uint32_t auth_data;

    ch9431_reset();                                     /* 软件复位 */

    _port_ch9431_spi_nss_low();                         /* 置CH9431的CS为低电平 */

    _port_ch9431_spi_rw_data(0);
    _port_ch9431_spi_rw_data(0xaa);
    _port_ch9431_spi_rw_data(0xaa);
    _port_ch9431_spi_rw_data(0x55);
    _port_ch9431_spi_rw_data(0xaa);
    _port_ch9431_spi_rw_data(0x55);

    _port_ch9431_spi_rw_data((uint8_t)(rand));
    _port_ch9431_spi_rw_data((uint8_t)(rand >> 8));
    _port_ch9431_spi_rw_data((uint8_t)(rand >> 16));
    _port_ch9431_spi_rw_data((uint8_t)(rand >> 24));

    _port_ch9431_spi_nss_high();                        /* 置CH9431的CS为高电平 */

    _port_ch9431_delay_ms(10);                          /* 等待CH9431处理 */

    _port_ch9431_spi_nss_low();                         /* 置CH9431的CS为低电平 */

    _port_ch9431_spi_rw_data(0);
    _port_ch9431_spi_rw_data(0x55);
    _port_ch9431_spi_rw_data(0x55);
    _port_ch9431_spi_rw_data(0xaa);
    _port_ch9431_spi_rw_data(0x55);
    _port_ch9431_spi_rw_data(0xaa);

    _port_ch9431_delay_us(CH9431_WAIT_DATA_US);

    read_data[0] = (uint8_t)_port_ch9431_spi_rw_data(0xff);
    read_data[1] = (uint8_t)_port_ch9431_spi_rw_data(0xff);
    read_data[2] = (uint8_t)_port_ch9431_spi_rw_data(0xff);
    read_data[3] = (uint8_t)_port_ch9431_spi_rw_data(0xff);

    _port_ch9431_spi_nss_high();                        /* 置CH9431的CS为高电平 */

    _port_ch9431_delay_ms(1);                           /* 等待系统处理 */

    auth_data = ((uint32_t)read_data[3] << 24) | ((uint32_t)read_data[2] << 16)
                | ((uint32_t)read_data[1] << 8) | (uint32_t)read_data[0];

    auth_data = auth_data - rand;

    _port_ch9431_spi_nss_low();                         /* 置CH9431的CS为低电平 */

    _port_ch9431_spi_rw_data(0);
    _port_ch9431_spi_rw_data(0xaa);
    _port_ch9431_spi_rw_data(0x5a);
    _port_ch9431_spi_rw_data(0x5a);
    _port_ch9431_spi_rw_data(0x5a);
    _port_ch9431_spi_rw_data(0x5a);

    _port_ch9431_delay_us(CH9431_WAIT_DATA_US);

    _port_ch9431_spi_rw_data((uint8_t)(auth_data));
    _port_ch9431_spi_rw_data((uint8_t)(auth_data >> 8));
    _port_ch9431_spi_rw_data((uint8_t)(auth_data >> 16));
    _port_ch9431_spi_rw_data((uint8_t)(auth_data >> 24));

    _port_ch9431_spi_nss_high();                        /* 置CH9431的CS为高电平 */

    _port_ch9431_delay_ms(10);                          /* 等待CH9431处理 */

    sysstat_t sysstat;
    sysstat.value = ch9431_read_byte(R8_SYSSTAT);

    return (sysstat.OPMOD == SYSCTRL_REQOP_IAP) ? 0 : 1;
}
