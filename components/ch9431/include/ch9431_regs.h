/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch9431_regs.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/10/10
 * Description        : This file contains ch9431 registers definitions and some parameters.
 *                      移植自 WCH CH9431EVT 的示例代码（EVT\EXAM\CH32F103\PUB\ch9431_inc.h），
 *                      仅替换头文件依赖，寄存器与位域定义保持 WCH 原样。
 * 移植者             : IT老大哥
 * 移植日期           : 2026-10-08
*********************************************************************************
* Copyright (c) 2023 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#ifndef _CH9431_REGS_H_
#define _CH9431_REGS_H_

#include <stdint.h>

/* LOAD_TX 命令的地址位：选择把 ID 写进哪个发送缓冲器 */
typedef enum
{
    CH9431_LOAD_TX_ID0 = 0,
    CH9431_LOAD_TX_ID1 = 2,
    CH9431_LOAD_TX_ID2 = 4,
} ch9431_load_tx_id_t;

/* LOAD_TX 命令的地址位：选择把数据写进哪个发送缓冲器 */
typedef enum
{
    CH9431_LOAD_TX_DATA0 = 1,
    CH9431_LOAD_TX_DATA1 = 3,
    CH9431_LOAD_TX_DATA2 = 5,
} ch9431_load_tx_data_t;

/* RD_RX_BUFF 命令的地址位：选择读哪个接收缓冲器的 ID */
typedef enum
{
    CH9431_READ_RX_ID0 = 0,
    CH9431_READ_RX_ID1 = 4,
} ch9431_read_rx_id_t;

/* RD_RX_BUFF 命令的地址位：选择读哪个接收缓冲器的数据 */
typedef enum
{
    CH9431_READ_RX_DATA0 = 2,
    CH9431_READ_RX_DATA1 = 6,
} ch9431_read_rx_data_t;

/* CAN SPI TIME */
#define CH9431_START_READY_MS               15
#define CH9431_WAKEUP_READY_MS              3

#define CH9431_CS_WAKEUP_US                 1
#define CH9431_CMD_CMD_US                   1
#define CH9431_READ_WAIT_DATA_US            1
#define CH9431_READ_BUF_DATA_US             1
#define CH9431_RD_RX_STATUS_US              1
#define CH9431_RTS_BUSY_US                  2

#define CH9431_WAIT_DATA_US                 1

/* CAN SPI commands */
#define CMD_CAN_WRITE                       0x02
#define CMD_CAN_READ                        0x03
#define CMD_CAN_BIT_MODIFY                  0x05

#define CMD_CAN_RD_STATUS                   0xa0
#define CMD_CAN_RX_STATUS                   0xb0

#define CMD_CAN_RESET                       0xc0

#define CMD_CAN_RTS                         0x80
#define CMD_CAN_LOAD_TX                     0X40
#define CMD_CAN_RD_RX_BUFF                  0x90

/* Configuration Registers */
#define R8_SYSCTRL                          0x0f
#define R8_SYSSTAT                          0x0e
#define R8_RXIPCTRL                         0x0c
#define R8_TXRTSCTRL                        0x0d

#define R8_TEC                              0x1c
#define R8_REC                              0x1d

#define R8_BTIMER3                          0x28
#define R8_BTIMER2                          0x29
#define R8_BTIMER1                          0x2a
#define R8_SYSINTE                          0x2b
#define R8_SYSINTF                          0x2c
#define R8_EFLAG                            0x2d

/*  Recieve Filters */
#define R8_RXF0SIDL                         0x00
#define R8_RXF0SIDH                         0x01
#define R8_RXF0EIDL                         0x02
#define R8_RXF0EIDH                         0x03

#define R8_RXF1SIDL                         0x04
#define R8_RXF1SIDH                         0x05
#define R8_RXF1EIDL                         0x06
#define R8_RXF1EIDH                         0x07

#define R8_RXF2SIDL                         0x08
#define R8_RXF2SIDH                         0x09
#define R8_RXF2EIDL                         0x0A
#define R8_RXF2EIDH                         0x0B

#define R8_RXF3SIDL                         0x10
#define R8_RXF3SIDH                         0x11
#define R8_RXF3EIDL                         0x12
#define R8_RXF3EIDH                         0x13

#define R8_RXF4SIDL                         0x14
#define R8_RXF4SIDH                         0x15
#define R8_RXF4EIDL                         0x16
#define R8_RXF4EIDH                         0x17

#define R8_RXF5SIDL                         0x18
#define R8_RXF5SIDH                         0x19
#define R8_RXF5EIDL                         0x1a
#define R8_RXF5EIDH                         0x1b

/* Receive Masks */
#define R8_RXM0SIDL                         0x20
#define R8_RXM0SIDH                         0x21
#define R8_RXM0EIDL                         0x22
#define R8_RXM0EIDH                         0x23

#define R8_RXM1SIDL                         0x24
#define R8_RXM1SIDH                         0x25
#define R8_RXM1EIDL                         0x26
#define R8_RXM1EIDH                         0x27

/* Tx Buffer 0 */
#define R8_TXB0CTRL                         0x30

#define R8_TXB0SIDL                         0x31
#define R8_TXB0SIDH                         0x32
#define R8_TXB0EIDL                         0x33
#define R8_TXB0EIDH                         0x34

#define R8_TXB0DLC                          0x35
#define R8_TXB0D0                           0x36
#define R8_TXB0D1                           0x37
#define R8_TXB0D2                           0x38
#define R8_TXB0D3                           0x39
#define R8_TXB0D4                           0x3A
#define R8_TXB0D5                           0x3B
#define R8_TXB0D6                           0x3C
#define R8_TXB0D7                           0x3D

/* Tx Buffer 1 */
#define R8_TXB1CTRL                         0x40

#define R8_TXB1SIDL                         0x41
#define R8_TXB1SIDH                         0x42
#define R8_TXB1EIDL                         0x43
#define R8_TXB1EIDH                         0x44

#define R8_TXB1DLC                          0x45
#define R8_TXB1D0                           0x46
#define R8_TXB1D1                           0x47
#define R8_TXB1D2                           0x48
#define R8_TXB1D3                           0x49
#define R8_TXB1D4                           0x4A
#define R8_TXB1D5                           0x4B
#define R8_TXB1D6                           0x4C
#define R8_TXB1D7                           0x4D

/* Tx Buffer 2 */
#define R8_TXB2CTRL                         0x50

#define R8_TXB2SIDL                         0x51
#define R8_TXB2SIDH                         0x52
#define R8_TXB2EIDL                         0x53
#define R8_TXB2EIDH                         0x54

#define R8_TXB2DLC                          0x55
#define R8_TXB2D0                           0x56
#define R8_TXB2D1                           0x57
#define R8_TXB2D2                           0x58
#define R8_TXB2D3                           0x59
#define R8_TXB2D4                           0x5A
#define R8_TXB2D5                           0x5B
#define R8_TXB2D6                           0x5C
#define R8_TXB2D7                           0x5D

/* Rx Buffer 0 */
#define R8_RXB0CTRL                         0x60

#define R8_RXB0SIDL                         0x61
#define R8_RXB0SIDH                         0x62
#define R8_RXB0EIDL                         0x63
#define R8_RXB0EIDH                         0x64

#define R8_RXB0DLC                          0x65
#define R8_RXB0D0                           0x66
#define R8_RXB0D1                           0x67
#define R8_RXB0D2                           0x68
#define R8_RXB0D3                           0x69
#define R8_RXB0D4                           0x6A
#define R8_RXB0D5                           0x6B
#define R8_RXB0D6                           0x6C
#define R8_RXB0D7                           0x6D

/* Rx Buffer 1 */
#define R8_RXB1CTRL                         0x70

#define R8_RXB1SIDL                         0x71
#define R8_RXB1SIDH                         0x72
#define R8_RXB1EIDL                         0x73
#define R8_RXB1EIDH                         0x74

#define R8_RXB1DLC                          0x75
#define R8_RXB1D0                           0x76
#define R8_RXB1D1                           0x77
#define R8_RXB1D2                           0x78
#define R8_RXB1D3                           0x79
#define R8_RXB1D4                           0x7A
#define R8_RXB1D5                           0x7B
#define R8_RXB1D6                           0x7C
#define R8_RXB1D7                           0x7D

#define SYSCTRL_REQOP_NORMAL                0
#define SYSCTRL_REQOP_CONFIGURATION         1
#define SYSCTRL_REQOP_LISTEN_ONLY           2
#define SYSCTRL_REQOP_LOOPBACK              3
#define SYSCTRL_REQOP_LIGHT_SLEEP           4
#define SYSCTRL_REQOP_DEEP_SLEEP            5
#define SYSCTRL_REQOP_IAP                   6

typedef __attribute__((aligned(1))) union __rxipctrl_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t B0BFM       : 1;
        uint8_t B1BFM       : 1;
        uint8_t B0BFE       : 1;
        uint8_t B1BFE       : 1;
        uint8_t B0BFS       : 1;
        uint8_t B1BFS       : 1;
        uint8_t             : 2;
    };
} rxipctrl_t;

typedef __attribute__((aligned(1))) union __txrtsctrl_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t B0RTSM      : 1;
        uint8_t B1RTSM      : 1;
        uint8_t B2RTSM      : 1;
        uint8_t B0RTS       : 1;
        uint8_t B1RTS       : 1;
        uint8_t B2RTS       : 1;
        uint8_t FIT         : 2;
    };
} txrtsctrl_t;

typedef __attribute__((aligned(1))) union __sysstat_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t             : 1;
        uint8_t ICOD        : 3;
        uint8_t             : 1;
        uint8_t OPMOD       : 3;
    };
} sysstat_t;

typedef __attribute__((aligned(1))) union __sysctrl_union_struct
{
    uint8_t value;

    struct
    {
        uint8_t CLKPRE      : 2;
        uint8_t CLKEN       : 1;
        uint8_t OSM         : 1;
        uint8_t ABAT        : 1;
        uint8_t REQOP       : 3;
    };
} sysctrl_t;

typedef __attribute__((aligned(1))) union __btimer3_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t TS2         : 3;
        uint8_t             : 5;
    };
} btimer3_t;

typedef __attribute__((aligned(1))) union __btimer2_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t TS1         : 4;
        uint8_t SJW         : 2;
        uint8_t             : 2;
    };
} btimer2_t;

typedef __attribute__((aligned(1))) union __btimer1_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t BRP         : 8;
    };
} btimer1_t;

typedef __attribute__((aligned(1))) union __sysint_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t RX0I        : 1;
        uint8_t RX1I        : 1;
        uint8_t TX0I        : 1;
        uint8_t TX1I        : 1;
        uint8_t TX2I        : 1;
        uint8_t ERRI        : 1;
        uint8_t WAKI        : 1;
        uint8_t MERR        : 1;
    };
} sysint_t;

typedef __attribute__((aligned(1))) union __eflag_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t EWARN       : 1;
        uint8_t RXWAR       : 1;
        uint8_t TXWAR       : 1;
        uint8_t RXEP        : 1;
        uint8_t TXEP        : 1;
        uint8_t TXBO        : 1;
        uint8_t RX0OVR      : 1;
        uint8_t RX1OVR      : 1;
    };
} eflag_t;

typedef __attribute__((aligned(1))) union __txbnctrl_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t TXREQ       : 1;
        uint8_t TXERR       : 1;
        uint8_t MLOA        : 1;
        uint8_t             : 5;
    };
} txbnctrl_t;

typedef __attribute__((aligned(1))) union __rxb0ctrl_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t FILHIT      : 1;
        uint8_t             : 1;
        uint8_t BUKT        : 1;
        uint8_t RXRTR       : 1;
        uint8_t             : 4;
    };
} rxb0ctrl_t;

typedef __attribute__((aligned(1))) union __rxb1ctrl_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t FILHIT      : 3;
        uint8_t             : 5;
    };
    struct
    {
        uint8_t FILHIT0     : 1;
        uint8_t FILHIT1     : 1;
        uint8_t FILHIT2     : 1;
        uint8_t RXRTR       : 1;
        uint8_t             : 4;
    };
} rxb1ctrl_t;

typedef __attribute__((aligned(1))) union __rxmn_union_struct
{
    struct
    {
        uint8_t     SIDL;
        uint8_t     SIDH;
        uint8_t     EIDL;
        uint8_t     EIDH;
    };
    struct
    {
        uint32_t    SID         : 11;
        uint8_t                 : 3;
        uint32_t    EID         : 18;
    };
} rxmn_t;

typedef __attribute__((aligned(1))) union __rxfn_union_struct
{
    struct
    {
        uint8_t     SIDL;
        uint8_t     SIDH;
        uint8_t     EIDL;
        uint8_t     EIDH;
    };
    struct
    {
        uint32_t    SID         : 11;
        uint8_t     EN          : 1;
        uint8_t     EXIDE       : 1;
        uint8_t                 : 1;
        uint32_t    EID         : 18;
    };
} rxfn_t;

typedef __attribute__((aligned(1))) union __rxbn_union_struct
{
    struct
    {
        uint8_t     SIDL;
        uint8_t     SIDH;
        uint8_t     EIDL;
        uint8_t     EIDH;
    };
    struct
    {
        uint32_t    SID         : 11;
        uint8_t                 : 1;
        uint8_t     IDE         : 1;
        uint8_t                 : 1;
        uint32_t    EID         : 18;
    };
} rxbn_t;

typedef __attribute__((aligned(1))) union __rxbndlc_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t     DLC         : 4;
        uint8_t                 : 2;
        uint8_t     RTR         : 1;
        uint8_t                 : 1;
    };
} rxbndlc_t;

typedef __attribute__((aligned(1))) union __txbndlc_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t     DLC         : 4;
        uint8_t                 : 2;
        uint8_t     RTR         : 1;
        uint8_t                 : 1;
    };
} txbndlc_t;

typedef __attribute__((aligned(1))) union __txbn_union_struct
{
    struct
    {
        uint8_t     SIDL;
        uint8_t     SIDH;
        uint8_t     EIDL;
        uint8_t     EIDH;
    };
    struct
    {
        uint32_t    SID         : 11;
        uint8_t                 : 1;
        uint8_t     EXIDE       : 1;
        uint8_t                 : 1;
        uint32_t    EID         : 18;
    };
} txbn_t;

typedef __attribute__((aligned(1))) union __rd_status_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t     RX0I        : 1;
        uint8_t     RX1I        : 1;
        uint8_t     TX0REQ      : 1;
        uint8_t     TX0I        : 1;
        uint8_t     TX1REQ      : 1;
        uint8_t     TX1I        : 1;
        uint8_t     TX2REQ      : 1;
        uint8_t     TX2I        : 1;
    };
} rd_status_t;

typedef __attribute__((aligned(1))) union __rx_status_union_struct
{
    uint8_t value;
    struct
    {
        uint8_t                 : 6;
        uint8_t     RX0I        : 1;
        uint8_t     RX1I        : 1;
    };
    struct
    {
        uint8_t     RXFn        : 3;
        uint8_t     RXRTR       : 1;
        uint8_t     RXIDE       : 1;
        uint8_t                 : 1;
        uint8_t     RXI         : 2;
    };
} rx_status_t;

typedef __attribute__((aligned(1))) union __load_tx_id_struct
{
    uint8_t value[5];
    struct
    {
        txbn_t      txbn;
        txbndlc_t   txbndlc;
    };
} load_tx_id_t;

typedef __attribute__((aligned(1))) union __load_tx_data_struct
{
    uint8_t     TXBnD[8];
    struct
    {
        uint8_t     TXBnD0;
        uint8_t     TXBnD1;
        uint8_t     TXBnD2;
        uint8_t     TXBnD3;
        uint8_t     TXBnD4;
        uint8_t     TXBnD5;
        uint8_t     TXBnD6;
        uint8_t     TXBnD7;
    };
} load_tx_data_t;

typedef __attribute__((aligned(1))) union __read_rx_id_struct
{
    uint8_t value[5];
    struct
    {
        rxbn_t      rxbn;
        rxbndlc_t   rxbndlc;
    };
} read_rx_id_t;

typedef __attribute__((aligned(1))) union __read_rx_data_struct
{
    uint8_t     RXBnD[8];
    struct
    {
        uint8_t     RXBnD0;
        uint8_t     RXBnD1;
        uint8_t     RXBnD2;
        uint8_t     RXBnD3;
        uint8_t     RXBnD4;
        uint8_t     RXBnD5;
        uint8_t     RXBnD6;
        uint8_t     RXBnD7;
    };
} read_rx_data_t;

/* 位域布局是协议的一部分，改动必须在这里编译期暴露出来 */
_Static_assert(sizeof(rxipctrl_t) == 1, "rxipctrl_t");
_Static_assert(sizeof(txrtsctrl_t) == 1, "txrtsctrl_t");
_Static_assert(sizeof(sysstat_t) == 1, "sysstat_t");
_Static_assert(sizeof(sysctrl_t) == 1, "sysctrl_t");
_Static_assert(sizeof(btimer1_t) == 1 && sizeof(btimer2_t) == 1 && sizeof(btimer3_t) == 1, "btimer");
_Static_assert(sizeof(sysint_t) == 1, "sysint_t");
_Static_assert(sizeof(eflag_t) == 1, "eflag_t");
_Static_assert(sizeof(txbnctrl_t) == 1, "txbnctrl_t");
_Static_assert(sizeof(rxb0ctrl_t) == 1 && sizeof(rxb1ctrl_t) == 1, "rxbnctrl_t");
_Static_assert(sizeof(rxbndlc_t) == 1 && sizeof(txbndlc_t) == 1, "dlc_t");
_Static_assert(sizeof(rd_status_t) == 1 && sizeof(rx_status_t) == 1, "status_t");
_Static_assert(sizeof(rxmn_t) == 4 && sizeof(rxfn_t) == 4 && sizeof(rxbn_t) == 4 && sizeof(txbn_t) == 4, "id_t");
_Static_assert(sizeof(load_tx_id_t) == 5 && sizeof(read_rx_id_t) == 5, "id_buf_t");
_Static_assert(sizeof(load_tx_data_t) == 8 && sizeof(read_rx_data_t) == 8, "data_buf_t");

#endif /* _CH9431_REGS_H_ */
