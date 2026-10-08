# ESP32-C3 + CH9431（SPI 转 CAN）

CH9431 是沁恒的 SPI 转 CAN 控制器（寄存器与 MCP2515 类似但不完全兼容），本工程把它做成一个 ESP-IDF 组件，
并在 ESP32-C3 上跑一个「每秒发一帧 + 中断收帧 + 打印总线错误计数」的示例。

驱动移植自 WCH CH9431EVT 的示例代码（CH32F103/PUB 下的 `ch9431_inc.h`、`ch9431_cmd.c`、`ch9431_example.c`），
原例程的 `CH9431_Xxx()` 命名统一改成 `ch9431_snake_case()`。**移植者：IT老大哥（2026-10-08）**。

## 目录结构

```
components/ch9431/
  ch9431.c              波特率/滤波器/掩码/工作模式/中断/帧收发
  ch9431_cmd.c          寄存器读写、软复位、SPI 快速收发命令
  ch9431_port.c/.h      ESP32 SPI + GPIO 移植层（私有头文件）
  include/ch9431.h      对外 API
  include/ch9431_regs.h 寄存器地址与位域定义（保留 WCH 的 R8_*/CMD_* 命名）
main/main.c             示例程序
```

## 硬件接线

| ESP32-C3 | CH9431 | 说明 |
|---|---|---|
| GPIO4 | SCK | SPI 时钟，示例用 8MHz |
| GPIO6 | MOSI (SI) | |
| GPIO5 | MISO (SO) | |
| GPIO7 | CS# | 软件片选 |
| GPIO10 | RESET# | 低有效 |
| GPIO3 | INT# | 收到帧/发送完成中断，下降沿 |
| GPIO1 | RX0IP | 接收缓冲器 0 满，可选 |
| GPIO0 | RX1IP | 接收缓冲器 1 满，可选 |
| 3V3 / GND | VCC / GND | |

SPI 参数：Mode 0（CPOL=0、CPHA=0）、MSB first、8MHz、软件 CS。
不需要的引脚（INT#、RX0IP、RX1IP、TXnRTS）在 `s_config` 里填 `-1` 即可，不接 INT 则退化为轮询。

CAN 侧注意：

- 总线两端各需一个 120Ω 终端电阻，否则容易出错误帧。
- **总线上必须有第二个节点回 ACK**，只有一块板子时发送会一直重发直到进入 bus-off（示例检测到 bus-off 会自动重新初始化）。
- 只有一块板子做验证时，把 `main.c` 里的 `CH9431_DEMO_LOOPBACK` 改成 `1`，用芯片内部回环自收自发。

## 环境要求

- ESP-IDF v6.1（v5.3 及以上应也可编译），目标芯片 **esp32c3**
- 组件依赖 `esp_driver_spi`、`esp_driver_gpio`（`idf_component_register` 里已声明）

## 编译与烧录

```bash
idf.py set-target esp32c3     # 已配置过可跳过；会重置 sdkconfig
idf.py build
idf.py -p COM3 flash monitor  # 换成自己的串口
```

- 产物：`build/ESP32C3_CH9431.bin`（约 170KB，默认单 app 分区 1MB，空间充足）
- `idf.py menuconfig` 可改日志级别、FreeRTOS tick 等
- Windows 上如果 `idf.py` 报找不到 python 环境，先用 ESP-IDF 命令提示符或 VS Code ESP-IDF 扩展的终端再执行

## 示例程序行为

`app_main()` 顺序：

1. `ch9431_port_init()` 装 SPI 总线 + GPIO，并复位芯片
2. `ch9431_init()` 复位 + 配波特率 + 6 个接收滤波器 + 2 个掩码 + 接收中断 + 切正常模式
3. `ch9431_set_bitrate(CH9431_DEMO_BITRATE)` 按宏配置波特率
4. 读版本串、`SYSSTAT`、`RD_STATUS`，把「接收缓冲器满」中断输出到 RX0IP/RX1IP 引脚
5. 起两个任务：`ch9431_rx`（等 NINT 信号量取帧）、`ch9431_tx`（每秒发一帧，标准帧/扩展帧、数据帧/远程帧轮流）

串口输出示例：

```
I (312) ch9431_demo: CH9431 version: V1.0.0
I (312) ch9431_demo: sysstat=0x00 (ICOD=0 OPMOD=0)
I (322) ch9431_demo: rd_status=0x00
I (322) ch9431: bitrate 100000 -> BRP=9 TS1=13 TS2=4
I (332) ch9431: init ok
I (1332) ch9431_demo: TX id=0x00000123 STD dlc=8 rtr=0 data=00 01 02 03 04 05 06 07
I (1332) ch9431_demo: eflag=0x00 tec=0 rec=0
```

## CAN 波特率

```c
#define CH9431_DEMO_BITRATE 500000   /* main.c 顶部，可填 1000000/500000/250000/125000 等 */
```

```c
esp_err_t ch9431_set_bitrate(uint32_t bitrate);   /* bps */
```

- 驱动会枚举 `BRP/TS1/TS2`，挑误差最小、采样点最接近 75% 的一组，约束是每比特 8~25 个 TQ。
- 设定值记在驱动内部的 `s_bitrate`，`ch9431_init()` 也用它 —— 所以 bus-off 后重新初始化不会退回默认值。
- **BTIMER1/2/3 只在配置模式下可写**，芯片在正常模式下会直接忽略这些写入。`ch9431_set_bitrate()` 内部会先切到配置模式、写完再切回原模式，所以任何模式下调用都生效；如果你绕过驱动直接写寄存器，记得先 `ch9431_set_mode(SYSCTRL_REQOP_CONFIGURATION)`。
- `CH9431_OSC_HZ`（`ch9431.h`）是 CAN 控制器的基准时钟，默认 20MHz。WCH 例程注释写 20MHz，EVT 原理图上晶振标 16M，按实际晶振改：若实际是 16MHz 而宏写 20MHz，输出的波特率会比目标值小 20%（例如设 500k 实际跑 400k）。

## 接收滤波器与掩码

芯片有 6 个接收滤波器和 2 个掩码，掩码决定「ID 的哪些位要参与比较」：

| 掩码位 | 含义 |
|---|---|
| 1 | 该位**不参与比较**（don't care） |
| 0 | 该位必须与滤波器的对应位相同 |

分组关系固定（`ch9431_init()` 里把 `RXB0CTRL.BUKT` 置 1，RXB0 满时滚存到 RXB1）：

| 掩码 | 管的滤波器 | 命中后进 |
|---|---|---|
| RXM0 (`ch9431_set_mask(0, ...)`) | RXF0、RXF1 | RXB0（满则滚到 RXB1） |
| RXM1 (`ch9431_set_mask(1, ...)`) | RXF2 ~ RXF5 | RXB1 |

**帧格式由滤波器决定，掩码管不了**：`ch9431_set_filter(..., extended, ...)` 的 `extended` 位写在滤波器里，
标准帧滤波器收不到扩展帧，反之亦然。想两种格式都收，就得分别配标准帧滤波器和扩展帧滤波器（示例里 6 个滤波器就是这么分的）。

### 常用配方

```c
/* 1) 只收一个标准帧 ID：掩码全 0，要求逐位相等 */
ch9431_set_filter(0, 0x123, 0, false, true);
ch9431_set_mask(0, 0, 0);

/* 2) 收所有标准帧：11 位 SID 全部不比较 */
ch9431_set_filter(0, 0, 0, false, true);
ch9431_set_mask(0, 0x7FF, 0);

/* 3) 只收 0x100 ~ 0x1FF：低 8 位不比较，高位必须等于 0x100 */
ch9431_set_filter(2, 0x100, 0, false, true);
ch9431_set_mask(1, 0x700, 0);

/* 4) 收扩展帧 0x1A0000 ~ 0x1A00FF：SID 固定，EID 只比较高 10 位 */
ch9431_set_filter(3, 6, 0x20000, true, true);   /* 29 位 ID 拆分：SID = ID>>18，EID = ID&0x3FFFF */
ch9431_set_mask(1, 0x7FF, 0xFF);

/* 5) 收全部（不挑 ID 只挑格式）：掩码全 1 */
ch9431_set_filter(0, 0, 0, false, true);
ch9431_set_filter(3, 0, 0, true, true);
ch9431_set_mask(0, 0x7FF, 0x3FFFF);
ch9431_set_mask(1, 0x7FF, 0x3FFFF);
```

几点提醒：

- 滤波器是「或」的关系：任一使能的滤波器命中就收进缓冲器。示例 `ch9431_init()` 默认把 6 个滤波器全部使能并只放行 `0x123` / `0x12345`，不想收的用 `ch9431_set_filter(idx, 0, 0, false, false)` 关掉。
- `RXF0/RXF1` 和 `RXF2 ~ RXF5` 分属不同掩码，改接收范围时注意别只改了滤波器忘了掩码（反之亦然）。
- 这两个函数和 `ch9431_set_bitrate()` 一样，内部会自动切配置模式再切回来，运行中随时可以改。
- 调试时读 `RXB1CTRL` 的 `FILHIT[2:0]`（`ch9431_read_byte(R8_RXB1CTRL)`，见 `rxb1ctrl_t`）可以看到命中的是几号滤波器。

## 主要 API

| 函数 | 说明 |
|---|---|
| `ch9431_port_init(config)` / `ch9431_port_deinit()` | 装/卸 SPI 总线与 GPIO |
| `ch9431_init()` | 复位 + 波特率 + 滤波器 + 掩码 + 中断 + 正常模式 |
| `ch9431_set_bitrate(bps)` | 改波特率，自动切配置模式 |
| `ch9431_set_filter()` / `ch9431_set_mask()` | 改接收滤波/掩码，自动切配置模式 |
| `ch9431_set_mode(reqop)` | 切 监听/回环/浅睡眠/深睡眠/IAP 模式，等 `SYSSTAT.OPMOD` 确认 |
| `ch9431_frame_send(&frame)` | 发一帧，自动挑空闲发送缓冲器（3 个都忙返回 `ESP_ERR_INVALID_STATE`） |
| `ch9431_frame_recv(&frame)` | 从 RXB0/RXB1 取一帧并清中断标志，无数据返回 `ESP_ERR_NOT_FOUND` |
| `ch9431_port_int_handler_set(handler, arg)` | 注册 NINT 下降沿回调（ISR 上下文，只做通知） |
| `ch9431_hard_reset()` / `ch9431_cs_wakeup()` | 硬复位 / 浅睡眠后拉 CS 唤醒 |
| `ch9431_send_buffer0/1/2()`、`ch9431_receive_buffer0/1()` | 原始收发，ID 用 `ch9431_init()` 写入的默认值 |
| `ch9431_enter_iap()` | 进入固件升级模式 |

## 常见问题

**版本串读出来是全 0 / `sysstat` 全是 0**
MISO 上没有数据。检查 MISO 接线、CS 是否接对、芯片供电（3.3V）。

**波特率、滤波掩码改了没反应**
这些寄存器（BTIMER1/2/3、RXF、RXM）**只在配置模式下可写**，正常工作模式下写进去会被静默忽略。
驱动里的 `ch9431_set_*` 已经自动处理；直接调 `ch9431_write_byte()` 时才需要注意。

**TEC 涨到 255、报 bus-off**
没有任何节点回 ACK：总线上只有一个节点、终端电阻缺失、两端波特率不一致，都会这样。示例检测到后会自动重新初始化，但根本问题还得从总线侧解决。

**收不到想收的 ID**
先确认掩码没有把要比较的位置 1（掩码 1 = 不比较），再确认滤波器使能且 `extended` 与该帧格式一致；
最后确认对端发出来的 ID 本身符合预期（标准帧只有 11 位）。

**发出去的帧自己收不到**
正常现象，除非开回环（`CH9431_DEMO_LOOPBACK 1`）或总线上有别的节点转发。

## 提交规范

遵循 [Conventional Commits](https://www.conventionalcommits.org/)，类型与含义：

| 类型 | 含义 |
|---|---|
| `feat` | 新增功能 |
| `fix` | 修复 bug |
| `docs` | 文档变更 |
| `style` | 代码格式（不影响功能） |
| `refactor` | 重构 |
| `perf` | 性能优化 |
| `test` | 测试相关 |
| `build` / `ci` | 构建系统 / CI 变更 |
| `chore` | 杂务 |
| `revert` | 回退 |

示例：`feat: 支持 1Mbps 波特率`、`fix(ch9431): 配置寄存器需先切配置模式`、`docs: 补充掩码使用说明`

打 tag 时按语义化版本：含 `feat` 升次版本号，只有 `fix`/`perf` 升修订号，不兼容变更（描述前加 `!`）升主版本号。

## 参考资料

- 沁恒 CH9431EVT 例程（`CH32F103/PUB`），本驱动的移植来源
- 沁恒 CH9431 数据手册（寄存器与位域定义以它为准）
- 寄存器地址与位域见 `components/ch9431/include/ch9431_regs.h`
