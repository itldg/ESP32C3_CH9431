/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * CH9431（SPI 转 CAN 控制器）在 ESP32-C3 上的使用示例，
 * 对应沁恒 CH9431EVT 的 SEND_RECV 例程：自发自收 + 状态/错误计数打印。
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "ch9431.h"

static const char *TAG = "ch9431_demo";

/*
 * ESP32-C3 与 CH9431 的接线：
 *
 *   GPIO4  -> SCK
 *   GPIO6  -> MOSI (SI)
 *   GPIO5  -> MISO (SO)
 *   GPIO7  -> CS#
 *   GPIO10 -> RESET#
 *   GPIO3  <- INT#
 *   GPIO1  <- RX0IP
 *   GPIO0  <- RX1IP
 *   3V3 / GND
 *
 * TX0RTS/TX1RTS/TX2RTS 本示例不使用（发送请求走寄存器），需要时把引脚填上即可。
 * INT#/RX0IP/RX1IP 是 CH9431 的输出，接 ESP32 输入即可。
 */
static const ch9431_config_t s_config = {
    .host = SPI2_HOST,
    .pin_sck = 4,
    .pin_mosi = 6,
    .pin_miso = 5,
    .pin_cs = 7,
    .pin_rst = 10,
    .pin_int = 3,
    .pin_rx0ip = 1,
    .pin_rx1ip = 0,
    .pin_tx0rts = -1,
    .pin_tx1rts = -1,
    .pin_tx2rts = -1,
    .clock_hz = 8 * 1000 * 1000,
};

static SemaphoreHandle_t s_rx_sem;
static bool s_bus_off;

/* 总线上没有第二个节点时可置 1：用芯片内部回环模式自收自发，验证收发通路 */
#define CH9431_DEMO_LOOPBACK 0

/* CAN 波特率，可填 1000000 / 500000 / 250000 / 125000 等 */
#define CH9431_DEMO_BITRATE 100000

static void ch9431_int_isr(void *arg)
{
    BaseType_t woken = pdFALSE;

    xSemaphoreGiveFromISR(s_rx_sem, &woken);
    if (woken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

/* 缓冲区满中断输出到 RX0IP、RX1IP 引脚；芯片复位后要重新设置 */
static void rx_int_to_pin(void)
{
    rxipctrl_t rxipctrl = {.B0BFE = 1, .B0BFM = 1, .B1BFE = 1, .B1BFM = 1, .B0BFS = 0, .B1BFS = 0};
    ch9431_write_byte(R8_RXIPCTRL, rxipctrl.value);
}

static void print_frame(const char *tag, const ch9431_frame_t *frame)
{
    char data[8 * 3 + 1];
    size_t pos = 0;

    data[0] = '\0';
    for (uint8_t i = 0; i < frame->dlc; i++)
    {
        pos += (size_t)snprintf(data + pos, sizeof(data) - pos, "%02X ", frame->data[i]);
    }

    ESP_LOGI(TAG, "%s id=0x%08" PRIX32 " %s dlc=%u rtr=%u data=%s",
             tag, frame->id, frame->extended ? "EXT" : "STD",
             frame->dlc, frame->rtr, data);
}

/* 收到 NINT 中断就取帧；超时也主动轮询一次，避免漏掉边沿 */
static void ch9431_rx_task(void *arg)
{
    ch9431_frame_t frame;

    for (;;)
    {
        xSemaphoreTake(s_rx_sem, pdMS_TO_TICKS(200));

        while (ch9431_frame_recv(&frame) == ESP_OK)
        {
            print_frame("RX", &frame);
        }
    }
}

/* 每秒发一帧，标准帧/扩展帧轮流，并打印总线错误计数 */
static void ch9431_tx_task(void *arg)
{
    ch9431_frame_t frame = {
        .id = 0x123,
        .extended = false,
        .rtr = false,
        .dlc = 8,
    };
    uint8_t counter = 0;

    for (;;)
    {
        frame.extended = (counter & 1) != 0;
        frame.id = frame.extended ? 0x12345 : 0x123;
        frame.rtr = (counter & 2) != 0;

        for (uint8_t i = 0; i < 8; i++)
        {
            frame.data[i] = (uint8_t)(counter + i);
        }

        esp_err_t err = ch9431_frame_send(&frame);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "TX failed: %s", esp_err_to_name(err));
        }
        else
        {
            print_frame("TX", &frame);
        }

        eflag_t eflag;
        eflag.value = ch9431_read_byte(R8_EFLAG);

        if (eflag.TXBO != 0)
        {
            /* 无人应答时发送错误计数会一路涨到 255 并进入 bus-off，只有复位才能退出 */
            if (!s_bus_off)
            {
                ESP_LOGW(TAG, "CAN bus-off (eflag=0x%02X tec=%u rec=%u)，总线无应答，重新初始化",
                         eflag.value, ch9431_read_byte(R8_TEC), ch9431_read_byte(R8_REC));
                s_bus_off = true;
            }
            if (ch9431_init() == ESP_OK)
            {
                rx_int_to_pin();
            }
            else
            {
                ESP_LOGE(TAG, "re-init failed");
            }
        }
        else
        {
            if (s_bus_off)
            {
                ESP_LOGI(TAG, "CAN bus recovered");
                s_bus_off = false;
            }
            ESP_LOGI(TAG, "eflag=0x%02X tec=%u rec=%u",
                     eflag.value, ch9431_read_byte(R8_TEC), ch9431_read_byte(R8_REC));
        }

        counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(ch9431_port_init(&s_config));

    /* 复位 + 波特率 + 滤波器 + 接收中断 + 正常工作模式 */
    ESP_ERROR_CHECK(ch9431_init());

    /* 换波特率：驱动内部会先切配置模式写完时序寄存器再切回正常模式 */
    ESP_ERROR_CHECK(ch9431_set_bitrate(CH9431_DEMO_BITRATE));
    ESP_LOGI(TAG, "CAN bitrate: %d bps", CH9431_DEMO_BITRATE);


    /* 版本串要在芯片复位就绪之后再读，正常是 7 个 ASCII 字符 + '\0' */
    uint8_t ver[8] = {0};
    ch9431_get_ver_string(ver);
    if (ver[7] == 0 && ver[0] != 0)
    {
        ESP_LOGI(TAG, "CH9431 version: %s", ver);
    }
    else
    {
        ESP_LOGW(TAG, "CH9431 version read failed: %02X %02X %02X %02X %02X %02X %02X %02X",
                 ver[0], ver[1], ver[2], ver[3], ver[4], ver[5], ver[6], ver[7]);
        ESP_LOGW(TAG, "全 0 表示 MISO 上没读到数据，请检查 MISO 接线/芯片供电");
    }

    sysstat_t sysstat;
    sysstat.value = ch9431_read_byte(R8_SYSSTAT);
    ESP_LOGI(TAG, "sysstat=0x%02X (ICOD=%u OPMOD=%u)", sysstat.value, sysstat.ICOD, sysstat.OPMOD);

    /* 把缓冲区满中断输出到 RX0IP、RX1IP 引脚 */
    rx_int_to_pin();

#if CH9431_DEMO_LOOPBACK
    /* 内部回环：自己发的帧自己收，不需要第二个节点和终端电阻 */
    if (ch9431_set_mode(SYSCTRL_REQOP_LOOPBACK) != ESP_OK)
    {
        ESP_LOGW(TAG, "loopback mode not confirmed, 仍按回环继续（看下面有没有 RX 日志）");
    }
    else
    {
        ESP_LOGI(TAG, "loopback mode enabled");
    }
#endif

    rd_status_t rd_status;
    ch9431_read_status(&rd_status);
    ESP_LOGI(TAG, "rd_status=0x%02X", rd_status.value);

    s_rx_sem = xSemaphoreCreateBinary();
    configASSERT(s_rx_sem != NULL);
    ESP_ERROR_CHECK(ch9431_port_int_handler_set(ch9431_int_isr, NULL));

    xTaskCreate(ch9431_rx_task, "ch9431_rx", 4096, NULL, 10, NULL);
    xTaskCreate(ch9431_tx_task, "ch9431_tx", 4096, NULL, 9, NULL);

    /*
     * 组件还提供了这些能力，按需调用：
     *   ch9431_set_bitrate()    换波特率（自动切配置模式，改完切回；bus-off 重初始化也沿用）
     *   ch9431_set_filter() / ch9431_set_mask()   改接收滤波
     *   ch9431_set_mode()       切换 监听/回环/浅睡眠/深睡眠/IAP 模式
     *   ch9431_cs_wakeup()      浅睡眠后拉 CS 唤醒
     *   ch9431_hard_reset()     RST 引脚硬复位
     *   ch9431_send_buffer0/1/2()、ch9431_receive_buffer0/1()   原始收发（ID 由初始化时设定）
     *   ch9431_load_tx_id()/load_tx_data()/read_rx_id()/read_rx_data()   SPI 快速收发命令
     *   ch9431_enter_iap()      进入固件升级模式
     */
}
