/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch9431_port.c
 * Author             : WCH / ESP-IDF port
 * Version            : V1.0.0
 * Description        : CH9431 在 ESP32 上的端口层：SPI 收发、CS/RST/RTS 引脚、延时。
 *                      移植自 WCH CH9431EVT 的示例代码（EVT\EXAM\CH32F103\PUB\ch9431_port.c），
 *                      原版基于 CH32 的 SPI1 + GPIO，此处改为 ESP-IDF spi_master，
 *                      并且不使用硬件 CS —— 协议要求 CS 跨越多个字节、且命令之间要留间隔，
 *                      因此由 _port_ch9431_spi_nss_* 手动控制。
 * 移植者             : IT老大哥
 * 移植日期           : 2026-10-08
*********************************************************************************
* Copyright (c) 2023 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"

#include "ch9431.h"
#include "ch9431_port.h"

static ch9431_config_t s_cfg;
static spi_device_handle_t s_spi;
static bool s_ready;

/* spi_device_polling_transmit 不可重入，且一条命令的字节流必须连续，
 * 所以用互斥锁把「CS 拉低 ~ 拉高」整段保护起来。 */
static SemaphoreHandle_t s_cmd_mutex;

static ch9431_int_handler_t s_int_handler;
static void *s_int_handler_arg;

/*********************************************************************
 * @fn      ch9431_gpio_isr
 *
 * @brief   INT# 下降沿的 GPIO 中断服务函数，只把事件转给注册的回调
 *          （回调在 ISR 上下文执行，里面不要做耗时操作）。
 *
 * @param   arg - 未使用。
 *
 * @return  none.
 */
static void ch9431_gpio_isr(void *arg)
{
    if (s_int_handler)
    {
        s_int_handler(s_int_handler_arg);
    }
}

/*********************************************************************
 * @fn      config_output
 *
 * @brief   把引脚配成输出并设一个初始电平；pin < 0 表示该引脚不用，直接返回。
 *
 * @param   pin   - GPIO 编号，负数表示不用这个脚。
 * @param   level - 初始电平（0/1）。
 *
 * @return  ESP_OK / gpio_config 的错误码。
 */
static esp_err_t config_output(int pin, int level)
{
    if (pin < 0)
    {
        return ESP_OK;
    }

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io);
    if (err == ESP_OK)
    {
        gpio_set_level(pin, level);
    }
    return err;
}

/*********************************************************************
 * @fn      config_input
 *
 * @brief   把引脚配成输入；pin < 0 表示该引脚不用，直接返回。
 *
 * @param   pin    - GPIO 编号，负数表示不用这个脚。
 * @param   intr   - 中断触发方式，不用中断填 GPIO_INTR_DISABLE。
 * @param   pullup - GPIO_PULLUP_ENABLE 时开内部上拉。
 *
 * @return  ESP_OK / gpio_config 的错误码。
 */
static esp_err_t config_input(int pin, gpio_int_type_t intr, gpio_pullup_t pullup)
{
    if (pin < 0)
    {
        return ESP_OK;
    }

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = pullup,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = intr,
    };
    return gpio_config(&io);
}

/*********************************************************************
 * @fn      ch9431_port_init
 *
 * @brief   装 SPI 总线 + GPIO，并建一条命令用的互斥锁。CS、RST 引脚必填，
 *          不用的脚填 -1；初始化完成后要自己调 ch9431_init() 复位并配置芯片。
 *
 * @param   config - 引脚与 SPI 配置，函数内会拷贝一份，可传栈上变量。
 *
 * @return  ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_STATE（已初始化过）/
 *          ESP_ERR_NO_MEM / SPI 初始化的错误码。
 */
esp_err_t ch9431_port_init(const ch9431_config_t *config)
{
    if (config == NULL || config->pin_cs < 0 || config->pin_rst < 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_ready)
    {
        return ESP_ERR_INVALID_STATE;
    }

    s_cfg = *config;
    if (s_cfg.clock_hz <= 0)
    {
        s_cfg.clock_hz = 8 * 1000 * 1000;
    }

    s_cmd_mutex = xSemaphoreCreateMutex();
    if (s_cmd_mutex == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    /* 引脚先就位：CS 拉高、RST 拉高（失能复位） */
    esp_err_t err = config_output(s_cfg.pin_cs, 1);
    if (err != ESP_OK)
    {
        return err;
    }
    err = config_output(s_cfg.pin_rst, 1);
    if (err != ESP_OK)
    {
        return err;
    }
    config_output(s_cfg.pin_tx0rts, 1);
    config_output(s_cfg.pin_tx1rts, 1);
    config_output(s_cfg.pin_tx2rts, 1);

    config_input(s_cfg.pin_rx0ip, GPIO_INTR_DISABLE, GPIO_PULLUP_ENABLE);
    config_input(s_cfg.pin_rx1ip, GPIO_INTR_DISABLE, GPIO_PULLUP_ENABLE);
    config_input(s_cfg.pin_int, GPIO_INTR_NEGEDGE, GPIO_PULLUP_ENABLE);

    spi_bus_config_t bus = {
        .mosi_io_num = s_cfg.pin_mosi,
        .miso_io_num = s_cfg.pin_miso,
        .sclk_io_num = s_cfg.pin_sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 16,
    };

    /* 每次事务最多 8 字节，用不到 DMA */
    err = spi_bus_initialize(s_cfg.host, &bus, SPI_DMA_DISABLED);
    if (err != ESP_OK)
    {
        vSemaphoreDelete(s_cmd_mutex);
        s_cmd_mutex = NULL;
        return err;
    }

    spi_device_interface_config_t dev = {
        .clock_speed_hz = s_cfg.clock_hz,
        .mode = 0,                  /* CPOL=0, CPHA=0，对应原例程 SPI_CPOL_Low + SPI_CPHA_1Edge */
        .spics_io_num = -1,         /* CS 手动控制 */
        .queue_size = 1,
    };

    err = spi_bus_add_device(s_cfg.host, &dev, &s_spi);
    if (err != ESP_OK)
    {
        spi_bus_free(s_cfg.host);
        vSemaphoreDelete(s_cmd_mutex);
        s_cmd_mutex = NULL;
        return err;
    }

    s_ready = true;
    return ESP_OK;
}

/*********************************************************************
 * @fn      ch9431_port_deinit
 *
 * @brief   移除 GPIO 中断、删除 SPI 设备并释放总线、删掉互斥锁，清空配置。
 *
 * @return  ESP_OK / ESP_ERR_INVALID_STATE（未初始化）。
 */
esp_err_t ch9431_port_deinit(void)
{
    if (!s_ready)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_cfg.pin_int >= 0)
    {
        gpio_isr_handler_remove(s_cfg.pin_int);
    }

    spi_bus_remove_device(s_spi);
    spi_bus_free(s_cfg.host);

    vSemaphoreDelete(s_cmd_mutex);
    s_cmd_mutex = NULL;

    s_spi = NULL;
    s_ready = false;
    s_int_handler = NULL;
    s_int_handler_arg = NULL;
    memset(&s_cfg, 0, sizeof(s_cfg));
    return ESP_OK;
}

/*********************************************************************
 * @fn      ch9431_port_config
 *
 * @brief   取当前生效的配置内容。
 *
 * @return  指向端口内部配置结构体的指针（只读，未初始化时为空值）。
 */
const ch9431_config_t *ch9431_port_config(void)
{
    return &s_cfg;
}

/*********************************************************************
 * @fn      ch9431_port_int_handler_set
 *
 * @brief   注册 INT# 下降沿回调。config.pin_int < 0 或 handler 为 NULL 时
 *          只记录不装中断，退化成纯轮询。
 *
 * @param   handler - ISR 上下文执行的回调，只做通知类操作（如给信号量）。
 * @param   arg     - 透传给回调的参数。
 *
 * @return  ESP_OK / GPIO 中断服务安装的错误码。
 */
esp_err_t ch9431_port_int_handler_set(ch9431_int_handler_t handler, void *arg)
{
    s_int_handler = handler;
    s_int_handler_arg = arg;

    if (handler == NULL || s_cfg.pin_int < 0)
    {
        return ESP_OK;
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        return err;
    }

    return gpio_isr_handler_add(s_cfg.pin_int, ch9431_gpio_isr, NULL);
}

/*********************************************************************
 * @fn      _port_ch9431_delay_us
 *
 * @brief   微秒级忙等延时，用于满足芯片要求的命令间隔、数据准备时间。
 *
 * @param   t - 延时长度，单位 us。
 *
 * @return  none.
 */
void _port_ch9431_delay_us(uint32_t t)
{
    esp_rom_delay_us(t);
}

/*********************************************************************
 * @fn      _port_ch9431_delay_ms
 *
 * @brief   毫秒级忙等延时（复位、唤醒等待用）。
 *
 * @param   t - 延时长度，单位 ms。
 *
 * @return  none.
 */
void _port_ch9431_delay_ms(uint32_t t)
{
    esp_rom_delay_us(t * 1000);
}

/*********************************************************************
 * @fn      _port_ch9431_rst_high
 *
 * @brief   拉高 RST 引脚，结束硬复位。
 *
 * @return  none.
 */
void _port_ch9431_rst_high(void)
{
    gpio_set_level(s_cfg.pin_rst, 1);
}

/*********************************************************************
 * @fn      _port_ch9431_rst_low
 *
 * @brief   拉低 RST 引脚，开始硬复位。
 *
 * @return  none.
 */
void _port_ch9431_rst_low(void)
{
    gpio_set_level(s_cfg.pin_rst, 0);
}

/*********************************************************************
 * @fn      _port_ch9431_spi_nss_high
 *
 * @brief   拉高 CS 结束一条命令，并释放命令互斥锁。
 *
 * @return  none.
 */
void _port_ch9431_spi_nss_high(void)
{
    gpio_set_level(s_cfg.pin_cs, 1);
    xSemaphoreGive(s_cmd_mutex);
}

/*********************************************************************
 * @fn      _port_ch9431_spi_nss_low
 *
 * @brief   取命令互斥锁、留出指令间隔、拉低 CS 开始一条命令。
 *          一条命令的所有字节必须在这对调用之间连续发出。
 *
 * @return  none.
 */
void _port_ch9431_spi_nss_low(void)
{
    xSemaphoreTake(s_cmd_mutex, portMAX_DELAY);
    _port_ch9431_delay_us(CH9431_CMD_CMD_US);   /* 保证两条指令之间的命令间隔 */
    gpio_set_level(s_cfg.pin_cs, 0);
}

/* 拉低 TX0RTS，用引脚方式请求发送 TXB0 */
void _port_ch9431_tx0rts_low(void)
{
    gpio_set_level(s_cfg.pin_tx0rts, 0);
}

/* 拉高 TX0RTS */
void _port_ch9431_tx0rts_high(void)
{
    gpio_set_level(s_cfg.pin_tx0rts, 1);
}

/* 拉低 TX1RTS，用引脚方式请求发送 TXB1 */
void _port_ch9431_tx1rts_low(void)
{
    gpio_set_level(s_cfg.pin_tx1rts, 0);
}

/* 拉高 TX1RTS */
void _port_ch9431_tx1rts_high(void)
{
    gpio_set_level(s_cfg.pin_tx1rts, 1);
}

/* 拉低 TX2RTS，用引脚方式请求发送 TXB2 */
void _port_ch9431_tx2rts_low(void)
{
    gpio_set_level(s_cfg.pin_tx2rts, 0);
}

/* 拉高 TX2RTS */
void _port_ch9431_tx2rts_high(void)
{
    gpio_set_level(s_cfg.pin_tx2rts, 1);
}

/*********************************************************************
 * @fn      _port_ch9431_spi_rw_data
 *
 * @brief   SPI 全双工交换一个字节（8bit、Mode 0），收发在一次事务里完成。
 *
 * @param   data - 要发送的字节。
 *
 * @return  收到的字节；SPI 事务失败时返回 0xff。
 */
uint16_t _port_ch9431_spi_rw_data(uint16_t data)
{
    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 8,
    };
    trans.tx_data[0] = (uint8_t)data;

    if (spi_device_polling_transmit(s_spi, &trans) != ESP_OK)
    {
        return 0xff;
    }
    return trans.rx_data[0];
}
