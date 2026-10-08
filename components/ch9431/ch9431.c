/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch9431.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/10/10
 * Description        : This file contains ch9431 command example.
 *                      移植自 WCH CH9431EVT 的示例代码（EVT\EXAM\CH32F103\PUB\ch9431_example.c），
 *                      并补上波特率/滤波器/掩码/工作模式配置与 ESP32 侧的帧收发封装，
 *                      函数名统一成 ch9431_snake_case。
 * 移植者             : IT老大哥
 * 移植日期           : 2026-10-08
*********************************************************************************
* Copyright (c) 2023 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#include <inttypes.h>
#include <string.h>

#include "esp_log.h"

#include "ch9431.h"
#include "ch9431_port.h"

static const char *TAG = "ch9431";

/* 发送缓冲器 0/1/2 的相关寄存器 */
static const uint8_t s_tx_ctrl[3] = { R8_TXB0CTRL, R8_TXB1CTRL, R8_TXB2CTRL };
static const uint8_t s_tx_sidl[3] = { R8_TXB0SIDL, R8_TXB1SIDL, R8_TXB2SIDL };
static const uint8_t s_tx_sidh[3] = { R8_TXB0SIDH, R8_TXB1SIDH, R8_TXB2SIDH };
static const uint8_t s_tx_eidl[3] = { R8_TXB0EIDL, R8_TXB1EIDL, R8_TXB2EIDL };
static const uint8_t s_tx_eidh[3] = { R8_TXB0EIDH, R8_TXB1EIDH, R8_TXB2EIDH };
static const uint8_t s_tx_dlc [3] = { R8_TXB0DLC,  R8_TXB1DLC,  R8_TXB2DLC  };
static const uint8_t s_tx_d0  [3] = { R8_TXB0D0,   R8_TXB1D0,   R8_TXB2D0   };

/* 接收滤波器/屏蔽器基地址：+0=SIDL, +1=SIDH, +2=EIDL, +3=EIDH */
static const uint8_t s_rxf_base[6] = { R8_RXF0SIDL, R8_RXF1SIDL, R8_RXF2SIDL,
                                       R8_RXF3SIDL, R8_RXF4SIDL, R8_RXF5SIDL };
static const uint8_t s_rxm_base[2] = { R8_RXM0SIDL, R8_RXM1SIDL };

/* 当前波特率：ch9431_init() 用它配置，ch9431_set_bitrate() 改完会更新 */
static uint32_t s_bitrate = 500000;

/*********************************************************************
 * @fn      config_mode_enter
 *
 * @brief   进入配置模式。BTIMER1/2/3、RXF0~5、RXM0/1 只在配置模式下可写，
 *          正常工作模式下写进去会被芯片忽略，所以改这些寄存器前先调它；
 *          已经在配置模式时不再切换，直接返回。
 *
 * @param   saved_mode - 输出，记下进入前的模式，写完用 config_mode_leave() 切回。
 *
 * @return  ESP_OK / 切模式失败的错误码。
 */
static esp_err_t config_mode_enter(uint8_t *saved_mode)
{
    sysstat_t sysstat;
    sysstat.value = ch9431_read_byte(R8_SYSSTAT);
    uint8_t mode = sysstat.OPMOD;

    if (mode > SYSCTRL_REQOP_IAP)
    {
        mode = SYSCTRL_REQOP_NORMAL;        /* OPMOD 不是合法模式，按正常模式恢复 */
    }
    *saved_mode = mode;

    if (mode == SYSCTRL_REQOP_CONFIGURATION)
    {
        return ESP_OK;
    }

    return ch9431_set_mode(SYSCTRL_REQOP_CONFIGURATION);
}

/*********************************************************************
 * @fn      config_mode_leave
 *
 * @brief   配置写完，把芯片切回 config_mode_enter() 记下的模式（本来就是配置
 *          模式则什么都不做）；切不回去只告警，不影响已经写好的配置。
 *
 * @param   saved_mode - config_mode_enter() 输出的原模式。
 *
 * @return  none.
 */
static void config_mode_leave(uint8_t saved_mode)
{
    if (saved_mode == SYSCTRL_REQOP_CONFIGURATION)
    {
        return;
    }

    esp_err_t err = ch9431_set_mode(saved_mode);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "restore mode %u failed: %s", saved_mode, esp_err_to_name(err));
    }
}

/*********************************************************************
 * @fn      write_bit_timing
 *
 * @brief   在配置模式下写 BTIMER1/2/3。按 CH9431_OSC_HZ 枚举 BRP/TS1/TS2，
 *          取波特率误差最小、采样点最接近 75% 的一组（每比特限 8~25 个 TQ）：
 *          波特率 = CH9431_OSC_HZ / ((TS1 + TS2 + 3) * (BRP + 1))
 *
 * @param   bitrate - 目标波特率，单位 bps。
 *
 * @return  ESP_OK / ESP_ERR_INVALID_ARG（找不到合法组合）。
 */
static esp_err_t write_bit_timing(uint32_t bitrate)
{
    uint32_t best_brp = 0, best_ts1 = 0, best_ts2 = 0;
    uint64_t best_err = UINT64_MAX;
    uint32_t best_sp_err = UINT32_MAX;
    bool found = false;

    for (uint32_t brp = 0; brp <= 255; brp++)
    {
        for (uint32_t ts1 = 0; ts1 <= 15; ts1++)
        {
            for (uint32_t ts2 = 0; ts2 <= 7; ts2++)
            {
                uint32_t tq = 1 + (ts1 + 1) + (ts2 + 1);        /* 每比特的 TQ 数 */
                if (tq < 8 || tq > 25)
                {
                    continue;                                   /* CAN 对每比特 TQ 数的要求 */
                }

                uint64_t actual = (uint64_t)CH9431_OSC_HZ * 1000 / ((uint64_t)tq * (brp + 1));
                uint64_t want = (uint64_t)bitrate * 1000;
                uint64_t err = actual > want ? actual - want : want - actual;

                /* 误差相同时，采样点取更接近 75% 的 */
                uint32_t sp = (1 + ts1 + 1) * 1000 / tq;
                uint32_t sp_err = sp > 750 ? sp - 750 : 750 - sp;

                if (found && (err > best_err || (err == best_err && sp_err >= best_sp_err)))
                {
                    continue;
                }

                found = true;
                best_err = err;
                best_sp_err = sp_err;
                best_brp = brp;
                best_ts1 = ts1;
                best_ts2 = ts2;
            }
        }
    }

    if (!found)
    {
        return ESP_ERR_INVALID_ARG;
    }

    btimer1_t btimer1 = { .value = 0 };
    btimer1.BRP = (uint8_t)best_brp;
    ch9431_write_byte(R8_BTIMER1, btimer1.value);
    _port_ch9431_delay_us(1);

    btimer2_t btimer2 = { .value = 0 };
    btimer2.SJW = 0;                        /* SJW 为 0，长度为 1TQ */
    btimer2.TS1 = (uint8_t)best_ts1;
    ch9431_write_byte(R8_BTIMER2, btimer2.value);
    _port_ch9431_delay_us(1);

    btimer3_t btimer3 = { .value = 0 };
    btimer3.TS2 = (uint8_t)best_ts2;
    ch9431_write_byte(R8_BTIMER3, btimer3.value);
    _port_ch9431_delay_us(1);

    ESP_LOGI(TAG, "bitrate %" PRIu32 " -> BRP=%" PRIu32 " TS1=%" PRIu32 " TS2=%" PRIu32,
             bitrate, best_brp, best_ts1, best_ts2);
    return ESP_OK;
}

/*********************************************************************
 * @fn      ch9431_set_bitrate
 *
 * @brief   改 CAN 波特率。芯片在正常工作模式下会忽略 BTIMER1/2/3 的写入，
 *          这里先切到配置模式写完再切回原来的模式，所以任何模式下调用都生效。
 *
 * @param   bitrate - 目标波特率，单位 bps。
 *
 * @return  ESP_OK / ESP_ERR_INVALID_ARG / 切模式失败的错误码。
 */
esp_err_t ch9431_set_bitrate(uint32_t bitrate)
{
    if (bitrate == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t saved_mode;
    esp_err_t err = config_mode_enter(&saved_mode);
    if (err != ESP_OK)
    {
        return err;
    }

    err = write_bit_timing(bitrate);
    if (err == ESP_OK)
    {
        s_bitrate = bitrate;
    }

    config_mode_leave(saved_mode);
    return err;
}

/*********************************************************************
 * @fn      ch9431_set_mode
 *
 * @brief   切换工作模式并等待 SYSSTAT.OPMOD 确认。
 *
 * @param   reqop - SYSCTRL_REQOP_xxx。
 *
 * @return  ESP_OK / ESP_ERR_TIMEOUT。
 */
esp_err_t ch9431_set_mode(uint8_t reqop)
{
    sysctrl_t sysctrl = { .value = 0 };
    sysctrl.REQOP = reqop;
    sysctrl.ABAT = 0;                   /* 不中止报文发送 */
    sysctrl.OSM = 0;                    /* 报文发送失败时自动重发 */
    sysctrl.CLKPRE = 1;                 /* 2 分频 */
    sysctrl.CLKEN = 1;                  /* CLKOUT 引脚使能 */

    uint8_t sysstat_value = 0;

    for (int retry = 0; retry < 200; retry++)
    {
        ch9431_write_byte(R8_SYSCTRL, sysctrl.value);
        _port_ch9431_delay_ms(2);

        sysstat_t sysstat;
        sysstat.value = ch9431_read_byte(R8_SYSSTAT);
        sysstat_value = sysstat.value;
        if (sysstat.OPMOD == reqop)
        {
            return ESP_OK;
        }
    }

    /* 芯片不一定会在 SYSSTAT.OPMOD 里回报所有模式，交给调用者决定是否继续 */
    ESP_LOGW(TAG, "mode %u not confirmed, SYSSTAT=0x%02X", reqop, sysstat_value);
    return ESP_ERR_TIMEOUT;
}

/*********************************************************************
 * @fn      ch9431_set_filter
 *
 * @brief   配置接收滤波器（EXIDE 决定该滤波器只收标准帧还是只收扩展帧）。
 *
 * @param   idx - 0 ~ 5。
 * @param   sid - 标准标识符，11 位。
 * @param   eid - 扩展标识符，18 位。
 * @param   extended - true 时该滤波器只作用于扩展帧。
 * @param   enable - 是否使能。
 *
 * @return  ESP_OK / ESP_ERR_INVALID_ARG / 切模式失败的错误码。
 */
esp_err_t ch9431_set_filter(uint8_t idx, uint32_t sid, uint32_t eid, bool extended, bool enable)
{
    if (idx > 5 || sid > 0x7FF || eid > 0x3FFFF)
    {
        return ESP_ERR_INVALID_ARG;
    }

    rxfn_t rxfn = { .SIDL = 0, .SIDH = 0, .EIDL = 0, .EIDH = 0 };
    rxfn.SID = sid;
    rxfn.EID = eid;
    rxfn.EXIDE = extended ? 1 : 0;
    rxfn.EN = enable ? 1 : 0;

    uint8_t saved_mode;
    esp_err_t err = config_mode_enter(&saved_mode);
    if (err != ESP_OK)
    {
        return err;
    }

    ch9431_write_byte(s_rxf_base[idx] + 0, rxfn.SIDL);
    _port_ch9431_delay_us(1);
    ch9431_write_byte(s_rxf_base[idx] + 1, rxfn.SIDH);
    _port_ch9431_delay_us(1);
    ch9431_write_byte(s_rxf_base[idx] + 2, rxfn.EIDL);
    _port_ch9431_delay_us(1);
    ch9431_write_byte(s_rxf_base[idx] + 3, rxfn.EIDH);
    _port_ch9431_delay_us(1);

    config_mode_leave(saved_mode);
    return ESP_OK;
}

/*********************************************************************
 * @fn      ch9431_set_mask
 *
 * @brief   配置接收屏蔽器，某位为 1 表示该位不参与比较（0 表示必须和滤波器一致）。
 *          RXM0 管 RXF0/RXF1（进 RXB0），RXM1 管 RXF2 ~ RXF5（进 RXB1）。
 *          帧格式由滤波器的 EXIDE 决定，掩码管不了。
 *
 * @param   idx - 0 或 1。
 * @param   sid - 标准标识符屏蔽，11 位。
 * @param   eid - 扩展标识符屏蔽，18 位。
 *
 * @return  ESP_OK / ESP_ERR_INVALID_ARG / 切模式失败的错误码。
 */
esp_err_t ch9431_set_mask(uint8_t idx, uint32_t sid, uint32_t eid)
{
    if (idx > 1 || sid > 0x7FF || eid > 0x3FFFF)
    {
        return ESP_ERR_INVALID_ARG;
    }

    rxmn_t rxmn = { .SIDL = 0, .SIDH = 0, .EIDL = 0, .EIDH = 0 };
    rxmn.SID = sid;
    rxmn.EID = eid;

    uint8_t saved_mode;
    esp_err_t err = config_mode_enter(&saved_mode);
    if (err != ESP_OK)
    {
        return err;
    }

    ch9431_write_byte(s_rxm_base[idx] + 0, rxmn.SIDL);
    _port_ch9431_delay_us(1);
    ch9431_write_byte(s_rxm_base[idx] + 1, rxmn.SIDH);
    _port_ch9431_delay_us(1);
    ch9431_write_byte(s_rxm_base[idx] + 2, rxmn.EIDL);
    _port_ch9431_delay_us(1);
    ch9431_write_byte(s_rxm_base[idx] + 3, rxmn.EIDH);
    _port_ch9431_delay_us(1);

    config_mode_leave(saved_mode);
    return ESP_OK;
}

/*********************************************************************
 * @fn      write_tx_id
 *
 * @brief   写入某个发送缓冲器的标识符：扩展帧拆成 11 位基本 ID + 18 位扩展 ID，
 *          标准帧只写 SID 并清 EXIDE。
 *
 * @param   n        - 发送缓冲器编号 0/1/2。
 * @param   id       - 帧 ID，标准帧 11 位、扩展帧 29 位。
 * @param   extended - true 表示扩展帧。
 *
 * @return  none.
 */
static void write_tx_id(uint8_t n, uint32_t id, bool extended)
{
    txbn_t txbn = { .SIDH = 0, .SIDL = 0, .EIDH = 0, .EIDL = 0 };

    if (extended)
    {
        txbn.SID = (id >> 18) & 0x7FF;      /* 29 位扩展帧：11 位基本 ID + 18 位扩展 ID */
        txbn.EID = id & 0x3FFFF;
    }
    else
    {
        txbn.SID = id & 0x7FF;
        txbn.EID = 0;
    }
    txbn.EXIDE = extended ? 1 : 0;

    ch9431_write_byte(s_tx_sidl[n], txbn.SIDL);
    _port_ch9431_delay_us(1);
    ch9431_write_byte(s_tx_sidh[n], txbn.SIDH);
    _port_ch9431_delay_us(1);
    ch9431_write_byte(s_tx_eidl[n], txbn.EIDL);
    _port_ch9431_delay_us(1);
    ch9431_write_byte(s_tx_eidh[n], txbn.EIDH);
    _port_ch9431_delay_us(1);
}

/*********************************************************************
 * @fn      send_buffer
 *
 * @brief   把数据写进发送缓冲器并置 TXREQ 请求发送。会先等该缓冲器的 TXREQ
 *          清零（最多 50ms），所以芯片正在发上一帧时这里会阻塞。
 *
 * @param   n   - 发送缓冲器编号 0/1/2。
 * @param   buf - 数据指针，rtr = 1（远程帧）时不写数据。
 * @param   len - 数据长度，超过 8 按 8 处理。
 * @param   rtr - 0 发数据帧，1 发远程帧。
 *
 * @return  ESP_OK / ESP_ERR_TIMEOUT（TXREQ 一直不清）。
 */
static esp_err_t send_buffer(uint8_t n, const uint8_t *buf, uint8_t len, uint8_t rtr)
{
    if (len > 8)
    {
        len = 8;
    }

    /* 等待 TXREQ 标志清零 */
    for (int dly = 0; ; dly++)
    {
        txbnctrl_t txbnctrl;
        txbnctrl.value = ch9431_read_byte(s_tx_ctrl[n]);
        if (txbnctrl.TXREQ == 0)
        {
            break;
        }
        if (dly > 50)
        {
            ESP_LOGE(TAG, "send %u failed: TXREQ busy", n);
            return ESP_ERR_TIMEOUT;
        }
        _port_ch9431_delay_ms(1);
    }

    if (rtr == 0)
    {
        for (uint8_t j = 0; j < len; j++)
        {
            ch9431_write_byte(s_tx_d0[n] + j, buf[j]);
        }
    }

    txbndlc_t txbndlc = { .value = 0 };
    txbndlc.DLC = len;
    txbndlc.RTR = rtr;
    ch9431_write_byte(s_tx_dlc[n], txbndlc.value);

    txbnctrl_t mask = { .value = 0 };
    txbnctrl_t value = { .value = 0 };
    mask.TXREQ = 1;
    value.TXREQ = 1;
    ch9431_bit_modify(s_tx_ctrl[n], mask.value, value.value);   /* 请求发送报文 */

    return ESP_OK;
}

/*********************************************************************
 * @fn      ch9431_frame_send
 *
 * @brief   发送一帧，自动挑选空闲的发送缓冲器（3 个都忙返回 ESP_ERR_INVALID_STATE）。
 *
 * @param   frame - 要发送的帧：id/extended/rtr/dlc/data。
 *
 * @return  ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_STATE。
 */
esp_err_t ch9431_frame_send(const ch9431_frame_t *frame)
{
    if (frame == NULL || frame->dlc > 8)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (frame->id > (frame->extended ? 0x1FFFFFFF : 0x7FF))
    {
        return ESP_ERR_INVALID_ARG;
    }

    int n = -1;
    for (int i = 0; i < 3; i++)
    {
        txbnctrl_t txbnctrl;
        txbnctrl.value = ch9431_read_byte(s_tx_ctrl[i]);
        if (txbnctrl.TXREQ == 0)
        {
            n = i;
            break;
        }
    }
    if (n < 0)
    {
        return ESP_ERR_INVALID_STATE;       /* 三个发送缓冲器都在用 */
    }

    write_tx_id((uint8_t)n, frame->id, frame->extended);

    return send_buffer((uint8_t)n, frame->data, frame->dlc, frame->rtr ? 1 : 0);
}

/*********************************************************************
 * @fn      ch9431_frame_recv
 *
 * @brief   从 RXB0/RXB1 取一帧并清掉对应中断标志。
 *
 * @return  ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_NOT_FOUND。
 */
esp_err_t ch9431_frame_recv(ch9431_frame_t *frame)
{
    if (frame == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    rx_status_t rx_status;
    ch9431_read_rx_status(&rx_status);

    uint8_t idx;
    if (rx_status.RX0I != 0)
    {
        idx = 0;
    }
    else if (rx_status.RX1I != 0)
    {
        idx = 1;
    }
    else
    {
        return ESP_ERR_NOT_FOUND;
    }

    read_rx_id_t rx_id;
    read_rx_data_t rx_data;

    ch9431_read_rx_id(idx ? CH9431_READ_RX_ID1 : CH9431_READ_RX_ID0, &rx_id);
    ch9431_read_rx_data(idx ? CH9431_READ_RX_DATA1 : CH9431_READ_RX_DATA0, &rx_data);

    if (rx_id.rxbndlc.DLC > 8)
    {
        /* DLC 只能是 0~8，读到 0xF 说明这次 SPI 读回来的全是 0xFF */
        ch9431_bit_modify(R8_SYSINTF, idx ? 0x02 : 0x01, 0);
        return ESP_ERR_INVALID_RESPONSE;
    }

    frame->dlc = rx_id.rxbndlc.DLC;
    frame->rtr = rx_id.rxbndlc.RTR != 0;
    frame->extended = rx_id.rxbn.IDE != 0;
    frame->id = frame->extended
                ? (((uint32_t)rx_id.rxbn.SID << 18) | rx_id.rxbn.EID)
                : rx_id.rxbn.SID;
    memcpy(frame->data, rx_data.RXBnD, frame->dlc);

    /* 用位操作指令清中断，防止清标志时丢掉新到的报文 */
    ch9431_bit_modify(R8_SYSINTF, idx ? 0x02 : 0x01, 0);

    return ESP_OK;
}

/*********************************************************************
 * @fn      ch9431_receive_buffer0
 *
 * @brief   轮询接收缓冲器 0：SYSINTF.RX0I 置位时读出 DLC 和 8 字节以内的数据。
 *          ID 请自行用 ch9431_read_rx_id()/读 RXB0SIDL~ 获取。
 *
 * @param   rx_buf  - 输出，数据缓冲区，至少 8 字节。
 * @param   rxbndlc - 输出，DLC/RTR 寄存器内容。
 *
 * @return  0 - 没有收到报文（RX0I 为 0）。
 *          1 - 收到一帧。
 */
uint8_t ch9431_receive_buffer0(uint8_t *rx_buf, rxbndlc_t *rxbndlc)
{
    sysint_t sysintf;

    sysintf.value = ch9431_read_byte(R8_SYSINTF);

    if (sysintf.RX0I == 0)
    {
        return 0;
    }

    rxbndlc->value = ch9431_read_byte(R8_RXB0DLC);

    if (rxbndlc->RTR == 0)      /* 判断非远程帧 */
    {
        uint8_t dlc = rxbndlc->DLC > 8 ? 8 : rxbndlc->DLC;

        for (uint8_t i = 0; i < dlc; i++)
        {
            rx_buf[i] = ch9431_read_byte(R8_RXB0D0 + i);        /* 把CAN接收到的数据放入指定缓冲区 */
        }
    }

    return 1;
}

/*********************************************************************
 * @fn      ch9431_receive_buffer1
 *
 * @brief   轮询接收缓冲器 1：SYSINTF.RX1I 置位时读出 DLC 和 8 字节以内的数据。
 *
 * @param   rx_buf  - 输出，数据缓冲区，至少 8 字节。
 * @param   rxbndlc - 输出，DLC/RTR 寄存器内容。
 *
 * @return  0 - 没有收到报文（RX1I 为 0）。
 *          1 - 收到一帧。
 */
uint8_t ch9431_receive_buffer1(uint8_t *rx_buf, rxbndlc_t *rxbndlc)
{
    sysint_t sysintf;

    sysintf.value = ch9431_read_byte(R8_SYSINTF);

    if (sysintf.RX1I == 0)
    {
        return 0;
    }

    rxbndlc->value = ch9431_read_byte(R8_RXB1DLC);

    if (rxbndlc->RTR == 0)      /* 判断非远程帧 */
    {
        uint8_t dlc = rxbndlc->DLC > 8 ? 8 : rxbndlc->DLC;

        for (uint8_t i = 0; i < dlc; i++)
        {
            rx_buf[i] = ch9431_read_byte(R8_RXB1D0 + i);        /* 把CAN接收到的数据放入指定缓冲区 */
        }
    }

    return 1;
}

/*********************************************************************
 * @fn      ch9431_send_buffer0
 *
 * @brief   用发送缓冲器 0 发一帧原始报文，ID 是 ch9431_init() 写进去的默认值。
 *
 * @param   tx_buf - 数据指针。
 * @param   len    - 数据长度（0~8）。
 * @param   rtr    - 0 发数据帧，1 发远程帧。
 *
 * @return  0 - 成功。
 *          0xff - 失败。
 */
uint8_t ch9431_send_buffer0(uint8_t *tx_buf, uint8_t len, uint8_t rtr)
{
    return send_buffer(0, tx_buf, len, rtr) == ESP_OK ? 0 : 0xff;
}

/*********************************************************************
 * @fn      ch9431_send_buffer1
 *
 * @brief   用发送缓冲器 1 发一帧原始报文，ID 是 ch9431_init() 写进去的默认值。
 *
 * @param   tx_buf - 数据指针。
 * @param   len    - 数据长度（0~8）。
 * @param   rtr    - 0 发数据帧，1 发远程帧。
 *
 * @return  0 - 成功。
 *          0xff - 失败。
 */
uint8_t ch9431_send_buffer1(uint8_t *tx_buf, uint8_t len, uint8_t rtr)
{
    return send_buffer(1, tx_buf, len, rtr) == ESP_OK ? 0 : 0xff;
}

/*********************************************************************
 * @fn      ch9431_send_buffer2
 *
 * @brief   用发送缓冲器 2 发一帧原始报文，ID 是 ch9431_init() 写进去的默认值。
 *
 * @param   tx_buf - 数据指针。
 * @param   len    - 数据长度（0~8）。
 * @param   rtr    - 0 发数据帧，1 发远程帧。
 *
 * @return  0 - 成功。
 *          0xff - 失败。
 */
uint8_t ch9431_send_buffer2(uint8_t *tx_buf, uint8_t len, uint8_t rtr)
{
    return send_buffer(2, tx_buf, len, rtr) == ESP_OK ? 0 : 0xff;
}

/*********************************************************************
 * @fn      ch9431_init
 *
 * @brief   复位芯片并完成基本配置：波特率（沿用上次 ch9431_set_bitrate() 的值,
 *          默认 500k）+ 发送缓冲器默认 ID + 接收滤波器/掩码 + 接收中断 + 正常模式。
 *          芯片始终没能进入正常模式时最多重试 3 次。
 *
 * @param   none.
 *
 * @return  ESP_OK - 初始化成功.
 *          ESP_FAIL - 芯片始终没能进入正常工作模式.
 *          ESP_ERR_INVALID_ARG - 波特率换算失败.
 */
esp_err_t ch9431_init(void)
{
    for (int attempt = 0; attempt < 3; attempt++)
    {
        ch9431_reset();             /* 发送复位指令软件复位 */

        /* 复位后芯片处于配置模式，直接写波特率（沿用上次 set_bitrate 设定的值） */
        if (write_bit_timing(s_bitrate) != ESP_OK)
        {
            return ESP_ERR_INVALID_ARG;
        }

        /* 设置报文发送ID 0/1/2，仅为了让 ch9431_send_bufferN() 开箱可用 */
        write_tx_id(0, 0x123, false);
        write_tx_id(1, 0x23, false);
        write_tx_id(2, 0x345, true);

        /* 设置TXRTS，是否支持使用PIN控制发送 */
        txrtsctrl_t txrtsctrl = { .value = 0 };
        txrtsctrl.FIT = 3;
        txrtsctrl.B0RTSM = 1;
        txrtsctrl.B1RTSM = 1;
        txrtsctrl.B2RTSM = 0;
        ch9431_bit_modify(R8_TXRTSCTRL, txrtsctrl.value, txrtsctrl.value);
        _port_ch9431_delay_us(1);

        /* 清空接收缓冲器0的标识符 */
        rxbn_t rxbn = { .SIDH = 0, .SIDL = 0, .EIDH = 0, .EIDL = 0 };
        ch9431_write_byte(R8_RXB0SIDL, rxbn.SIDL);
        _port_ch9431_delay_us(1);
        ch9431_write_byte(R8_RXB0SIDH, rxbn.SIDH);
        _port_ch9431_delay_us(1);
        ch9431_write_byte(R8_RXB0EIDL, rxbn.EIDL);
        _port_ch9431_delay_us(1);
        ch9431_write_byte(R8_RXB0EIDH, rxbn.EIDH);
        _port_ch9431_delay_us(1);

        /* RXB0 满时滚存到 RXB1 */
        rxb0ctrl_t rxb0ctrl = { .value = 0 };
        rxb0ctrl.BUKT = 1;
        ch9431_write_byte(R8_RXB0CTRL, rxb0ctrl.value);
        _port_ch9431_delay_us(1);

        /* 接收滤波器 0 ~ 5：RXF0~RXF2 作用于标准帧，RXF3~RXF5 作用于扩展帧 */
        for (uint8_t i = 0; i < 6; i++)
        {
            ch9431_set_filter(i, 0x123, 0x12345, i >= 3, true);
        }

        /* 接收屏蔽器 0/1 全 0：所有位都参与比较，即只接收与滤波器完全一致的报文 */
        ch9431_set_mask(0, 0, 0);
        ch9431_set_mask(1, 0, 0);

        /* 默认清空中断标志位（必须由MCU清空） */
        sysint_t sysintf = { .value = 0 };
        ch9431_write_byte(R8_SYSINTF, sysintf.value);

        /* 设置中断使能位：接收缓冲器 0/1 满中断 */
        sysint_t sysinte = { .value = 0 };
        sysinte.RX0I = 1;
        sysinte.RX1I = 1;
        ch9431_write_byte(R8_SYSINTE, sysinte.value);

        if (ch9431_set_mode(SYSCTRL_REQOP_NORMAL) == ESP_OK)
        {
            ESP_LOGI(TAG, "init ok");
            return ESP_OK;
        }

        ESP_LOGW(TAG, "chip error, retry init");
    }

    return ESP_FAIL;
}
