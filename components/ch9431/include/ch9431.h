/*
 * CH9431 SPI-CAN 控制器驱动（ESP-IDF 组件）
 *
 * 移植自 WCH CH9431EVT 的示例代码（EVT\EXAM\CH32F103\PUB\ch9431_cmd.c、ch9431_example.c），
 * 原例程的 CH9431_Xxx() 命名已统一为 ch9431_snake_case。
 *
 * 移植者  ：IT老大哥
 * 移植日期：2026-10-08
 */
#ifndef _CH9431_H_
#define _CH9431_H_

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"
#include "driver/spi_master.h"

#include "ch9431_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CH9431 模块上 CAN 控制器的基准时钟（晶振频率）。
 * WCH 例程注释写的是 20MHz，EVT 原理图上晶振标 16M，仅影响波特率换算，
 * 换成实际频率即可。
 */
#ifndef CH9431_OSC_HZ
#define CH9431_OSC_HZ 20000000
#endif

/* ============================ ESP32 胶水层 ============================ */

typedef enum
{
    CH9431_DISABLE = 0,
    CH9431_ENABLE  = 1,
} ch9431_enable_t;

typedef struct
{
    spi_host_device_t host;      /* SPI2_HOST / SPI3_HOST */
    int pin_sck;
    int pin_mosi;
    int pin_miso;
    int pin_cs;                  /* 片选，由驱动手动控制，必填 */
    int pin_rst;                 /* 复位，低有效，必填 */
    int pin_int;                 /* NINT 中断输入，不用填 -1 */
    int pin_rx0ip;               /* 不用填 -1 */
    int pin_rx1ip;
    int pin_tx0rts;              /* 不用填 -1 */
    int pin_tx1rts;
    int pin_tx2rts;
    int clock_hz;                /* SPI 时钟，填 0 时用 8MHz */
} ch9431_config_t;

typedef struct
{
    uint32_t id;                 /* 标准帧 11 位，扩展帧 29 位 */
    bool     extended;
    bool     rtr;
    uint8_t  dlc;                /* 0 ~ 8 */
    uint8_t  data[8];
} ch9431_frame_t;

/* 中断回调在 GPIO ISR 上下文执行，只能做通知类操作（如给出信号量） */
typedef void (*ch9431_int_handler_t)(void *arg);

/* 装 SPI 总线 + GPIO（不含芯片初始化，芯片侧要自己调 ch9431_init()） */
esp_err_t ch9431_port_init(const ch9431_config_t *config);

/* 卸载 SPI 总线与 GPIO，释放互斥锁 */
esp_err_t ch9431_port_deinit(void);

/* 取当前生效的配置（只读） */
const ch9431_config_t *ch9431_port_config(void);

/* 注册 NINT 下降沿回调，config.pin_int < 0 或 handler 为 NULL 时只记录不安装 */
esp_err_t ch9431_port_int_handler_set(ch9431_int_handler_t handler, void *arg);

/* 发送一帧，自动挑选空闲的发送缓冲器；三个都忙返回 ESP_ERR_INVALID_STATE */
esp_err_t ch9431_frame_send(const ch9431_frame_t *frame);

/* 从 RXB0/RXB1 取一帧并清除对应中断标志；无数据返回 ESP_ERR_NOT_FOUND */
esp_err_t ch9431_frame_recv(ch9431_frame_t *frame);

/* ============================ 驱动移植层 ============================ */
/* 下面这些函数移植自 WCH CH9431EVT 的示例代码，命名保持原样（接口在 ch9431_cmd.c） */

/* 软件复位（RESET 命令）并等芯片启动，复位后芯片处于配置模式 */
void     ch9431_reset(void);

/* 拉 RST 引脚硬复位并等芯片启动 */
void     ch9431_hard_reset(void);

/* 拉低 CS 一小段时间，把浅睡眠的芯片唤醒 */
void     ch9431_cs_wakeup(void);

/* 写/读一个寄存器字节 */
void     ch9431_write_byte(uint8_t addr, uint8_t value);
uint8_t  ch9431_read_byte(uint8_t addr);

/* 按位改寄存器：mask 里为 1 的位才用 value 改写 */
void     ch9431_bit_modify(uint8_t addr, uint8_t mask, uint8_t value);

/* 快速读状态字（TXB 空闲 / RXB 有报文），比逐个读寄存器快 */
void     ch9431_read_status(rd_status_t *rd_status);

/* 快速读接收状态，看 RX0I/RX1I 哪个接收缓冲器有报文 */
void     ch9431_read_rx_status(rx_status_t *rx_status);

/* 请求发送 TXB0/1/2，参数传 CH9431_ENABLE 表示请求该缓冲器发送 */
void     ch9431_rts(ch9431_enable_t txb0, ch9431_enable_t txb1, ch9431_enable_t txb2);

/* SPI 快速命令：直接读写 TXBn/RXBn 的 ID 和数据，不经寄存器地址 */
void     ch9431_load_tx_id(ch9431_load_tx_id_t opt, load_tx_id_t *tx_id);
void     ch9431_load_tx_data(ch9431_load_tx_data_t opt, load_tx_data_t *tx_data);
void     ch9431_read_rx_id(ch9431_read_rx_id_t opt, read_rx_id_t *read_rx_id);
void     ch9431_read_rx_data(ch9431_read_rx_data_t opt, read_rx_data_t *read_rx_data);

/* 读 8 字节版本字符串 */
void     ch9431_get_ver_string(uint8_t *ver_str);

/* 进入 IAP 升级模式，0 成功，1 失败 */
uint8_t  ch9431_enter_iap(uint32_t rand);

/*
 * 改 CAN 波特率（bps）。BTIMER1/2/3 只在配置模式下可写，这里会自动切到配置模式
 * 写完再切回原来的模式，因此在正常模式下调用也生效。新值会被 ch9431_init() 沿用。
 */
esp_err_t ch9431_set_bitrate(uint32_t bitrate);

/*
 * 切换工作模式（SYSCTRL_REQOP_xxx），会等待并确认 SYSSTAT.OPMOD。
 * 部分模式芯片不上报 OPMOD，此时返回 ESP_ERR_TIMEOUT 但模式可能已生效，调用者可自行决定是否继续。
 */
esp_err_t ch9431_set_mode(uint8_t reqop);

/*
 * 接收滤波器，idx = 0 ~ 5。extended 决定该滤波器只收扩展帧还是只收标准帧，
 * 掩码管不了帧格式。RXF0/RXF1 → RXB0，RXF2 ~ RXF5 → RXB1。
 */
esp_err_t ch9431_set_filter(uint8_t idx, uint32_t sid, uint32_t eid, bool extended, bool enable);

/*
 * 接收屏蔽器，idx = 0（管 RXF0/RXF1）或 1（管 RXF2 ~ RXF5）。
 * 掩码某位为 1 = 该位不参与比较；为 0 = 必须和滤波器的对应位相同。
 * 两者都在配置模式下才能写，函数内部会自动切过去再切回来。
 */
esp_err_t ch9431_set_mask(uint8_t idx, uint32_t sid, uint32_t eid);

/* 复位 + 配置波特率（默认 500kbps，可用 ch9431_set_bitrate() 改）+ 滤波器 + 中断 + 正常模式 */
esp_err_t ch9431_init(void);

/* 原始收发：ID 用 ch9431_init() 写入的默认值，数据长度/rtr 由调用者给出 */
uint8_t  ch9431_receive_buffer0(uint8_t *rx_buf, rxbndlc_t *rxbndlc);
uint8_t  ch9431_receive_buffer1(uint8_t *rx_buf, rxbndlc_t *rxbndlc);
uint8_t  ch9431_send_buffer0(uint8_t *tx_buf, uint8_t len, uint8_t rtr);
uint8_t  ch9431_send_buffer1(uint8_t *tx_buf, uint8_t len, uint8_t rtr);
uint8_t  ch9431_send_buffer2(uint8_t *tx_buf, uint8_t len, uint8_t rtr);

#ifdef __cplusplus
}
#endif

#endif /* _CH9431_H_ */
