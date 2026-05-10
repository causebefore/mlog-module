# MLog

一个轻量级、高性能的嵌入式日志库，专为嵌入式系统和微控制器设计。

## 目录

- [项目简介](#项目简介)
- [Python 串口日志工具](#python-串口日志工具)
- [主要特性](#主要特性)
- [快速开始](#快速开始)
- [配置说明](#配置说明)
- [API 使用示例](#api-使用示例)
- [移植指南](#移植指南)
- [许可证](#许可证)

## 项目简介

MLog 是一个专为嵌入式系统设计的 C 语言日志库，提供了灵活的日志输出功能。它具有以下特点：

- **轻量级设计**：最小化内存占用，适合资源受限的嵌入式环境
- **高度可配置**：通过编译时配置，可以根据项目需求定制功能
- **平台无关**：通过移植层接口，可以轻松适配到不同的硬件平台
- **性能优化**：支持缓冲输出模式，减少 I/O 调用次数，提高性能

## 项目初始化

本仓库已提供最小可用初始化工程，默认包含：

- 核心静态库构建入口：`CMakeLists.txt`
- 裸机最小示例：`examples/baremetal_minimal/main.c`
- 可替换的移植层示例：`examples/baremetal_minimal/mlog_port_minimal.c`

### 1. 生成构建目录

```bash
cmake -S . -B build
```

### 2. 编译

```bash
cmake --build build
```

编译后会生成：

- `mlog` 静态库
- `mlog_example` 示例可执行文件（用于验证初始化流程）

### 3. 在 Cortex-M 裸机工程中接入

1. 将 `inc/` 与 `src/` 加入你的工程。
2. 参考 `examples/baremetal_minimal/mlog_port_minimal.c` 实现平台输出：
   - 重写 `mlog_port_hw_write()`，对接 UART/SWO/RTT。
   - 可选重写 `mlog_port_irq_save()/mlog_port_irq_restore()` 实现临界区保护。
3. 启动阶段调用：

```c
mlog_port_register(mlog_port_get_default());
mlog_init();
mlog_start();
```

## Python 串口日志工具

仓库提供 `tools/mlog_serial_logger.py`，用于配合 MLog 串口输出记录本地日志。工具会同时生成原始日志和 agent 易读的结构化日志：

- `logs/mlog_<session>.raw.log`：串口原始字节，完整保留 ANSI 颜色和换行。
- `logs/mlog_<session>.jsonl`：逐行 JSONL，agent 优先读取该文件。
- `logs/mlog_<session>.meta.json`：采集会话元信息，包含串口、波特率、状态、字节数、行数和文件路径。

安装依赖：

```bash
pip install -r requirements.txt
```

前台调试：

```bash
python tools/mlog_serial_logger.py run --port COM3 --baud 115200
```

后台采集并输出 JSON：

```bash
python tools/mlog_serial_logger.py ports --json
python tools/mlog_serial_logger.py start --port COM3 --baud 115200 --json
python tools/mlog_serial_logger.py status --json
python tools/mlog_serial_logger.py tail --lines 20 --json
python tools/mlog_serial_logger.py stop --json
```

给 agent 使用时，应优先读取 `meta.json` 中的 `files.jsonl` 路径，不要解析人类可读的控制台输出。JSONL 每行包含：

- `seq`：行序号。
- `pc_time`：PC 接收时间。
- `text`：去掉 ANSI 颜色和行尾换行后的文本。
- `raw_offset` / `raw_len`：该行在 `.raw.log` 中的原始字节位置。
- `complete`：是否收到完整换行。

仓库同时提供 `.agents/skills/mlog-serial-control/SKILL.md`，agent 需要采集、查询或停止串口日志时可使用 `$mlog-serial-control`。

## 主要特性

### 1. 多级日志级别

支持 6 个日志级别，满足不同场景的需求：

- **ASSERT (断言)**：用于断言失败时的日志输出
- **ERROR (错误)**：严重错误，需要立即处理
- **WARN (警告)**：警告信息，可能存在问题
- **INFO (信息)**：重要的运行信息
- **DEBUG (调试)**：调试信息，用于开发阶段
- **VERBOSE (详细)**：最详细的日志信息

### 2. 灵活的过滤机制

- **级别过滤**：设置全局日志级别，只输出指定级别及以上的日志
- **标签过滤**：按标签筛选日志输出
- **标签级别过滤**：为不同标签设置不同的日志级别

### 3. 丰富的格式化选项

可自定义日志输出格式，支持以下信息：

- 日志级别标识
- 标签（Tag）
- 时间戳
- 进程信息
- 线程信息
- 文件路径和名称
- 函数名
- 行号

### 4. 彩色输出支持

- 支持 ANSI 转义序列彩色输出
- 不同级别使用不同颜色，便于快速识别
- 可在运行时启用/禁用颜色输出

### 5. 缓冲输出模式

- 高频率日志场景下，使用环形缓冲减少输出设备调用
- 按 FIFO 顺序刷新，满时丢弃新写入的数据并记录溢出统计
- 支持手动全量刷新或分块刷新缓冲区

### 6. 实用工具功能

- **断言机制**：内置断言功能，支持自定义断言钩子
- **十六进制转储**：方便查看二进制数据
- **线程安全**：通过锁机制保证多线程环境下的安全性

### 7. 高度可移植

- 核心代码与平台无关
- 通过移植层接口适配不同平台
- 支持裸机、RTOS 等多种运行环境

## 快速开始

### 目录结构

```txt
mlog-module/
├── inc/              # 头文件目录
│   ├── mlog.h       # 主头文件，包含 API 声明
│   └── mlog_cfg.h   # 配置文件
├── src/              # 源代码目录
│   ├── mlog.c       # 核心实现
│   └── mlog_buf.c   # 缓冲输出实现
└── port/             # 移植层目录
    └── mlog_port.c  # 移植层实现模板
```

### 基本使用步骤

#### 1. 添加源文件到项目

将以下文件添加到您的项目中：

- `inc/mlog.h`
- `inc/mlog_cfg.h`
- `src/mlog.c`
- `src/mlog_buf.c`（如果需要缓冲输出）
- `port/mlog_port.c`

#### 2. 配置编译选项

在项目的包含路径中添加 `inc` 目录：

```makefile
CFLAGS += -I/path/to/mlog-module/inc
```

#### 3. 实现移植层接口

在 `port/mlog_port.c` 中实现平台相关的函数（详见[移植指南](#移植指南)）。

#### 4. 初始化和使用

```c
#include <mlog.h>

int main(void)
{
    // 注册移植层接口
    mlog_port_register(mlog_port_get_default());

    // 初始化 MLog
    mlog_init();

    // 启动日志输出
    mlog_start();

    // 使用日志功能
    mlog_i("main", "Hello, MLog!");
    mlog_d("main", "Debug message: value = %d", 42);
    mlog_w("main", "Warning: Low memory");
    mlog_e("main", "Error occurred!");

    // 停止日志输出
    mlog_stop();

    // 反初始化
    mlog_deinit();

    return 0;
}
```

### 简化的日志宏

MLog 提供了简化的日志宏，可以在文件开头定义 `LOG_TAG` 和 `LOG_LVL`：

```c
#define LOG_TAG "MyModule"
#define LOG_LVL MLOG_LVL_DEBUG
#include <mlog.h>

void my_function(void)
{
    log_i("Initialization complete");  // 自动使用 LOG_TAG
    log_d("Counter: %d", counter);
    log_e("Failed to open file");
}
```

## 配置说明

### mlog_cfg.h 配置选项

#### 1. 基本功能开关

```c
#define MLOG_OUTPUT_ENABLE    // 使能日志输出功能
#define MLOG_ASSERT_ENABLE    // 使能断言检查功能
```

#### 2. 日志级别设置

```c
// 设置编译时的最高日志级别
// 可选值：MLOG_LVL_ASSERT, MLOG_LVL_ERROR, MLOG_LVL_WARN,
//        MLOG_LVL_INFO, MLOG_LVL_DEBUG, MLOG_LVL_VERBOSE
#define MLOG_OUTPUT_LVL MLOG_LVL_VERBOSE
```

#### 3. 缓冲区大小配置

```c
#define MLOG_LINE_BUF_SIZE 256           // 单行日志缓冲区大小（字节）
#define MLOG_LINE_NUM_MAX_LEN 5          // 行号最大长度
#define MLOG_FILTER_TAG_MAX_LEN 15       // 标签最大长度
#define MLOG_FILTER_TAG_LVL_MAX_NUM 5    // 标签级别过滤器最大数量
```

#### 4. 输出格式配置

```c
#define MLOG_NEWLINE_SIGN "\n"           // 换行符（可改为 "\r\n"）

// 启用以下宏来在日志中包含对应信息
#define MLOG_FMT_USING_FUNC              // 包含函数名
#define MLOG_FMT_USING_DIR               // 包含文件路径
#define MLOG_FMT_USING_LINE              // 包含行号
```

#### 5. 颜色配置

```c
#define MLOG_COLOR_ENABLE                // 使能彩色输出

// 自定义各级别的颜色（使用 ANSI 转义序列）
#define MLOG_COLOR_ASSERT  (F_MAGENTA B_NULL S_NORMAL)
#define MLOG_COLOR_ERROR   (F_RED B_NULL S_NORMAL)
#define MLOG_COLOR_WARN    (F_YELLOW B_NULL S_NORMAL)
#define MLOG_COLOR_INFO    (F_CYAN B_NULL S_NORMAL)
#define MLOG_COLOR_DEBUG   (F_GREEN B_NULL S_NORMAL)
#define MLOG_COLOR_VERBOSE (F_BLUE B_NULL S_NORMAL)
```

#### 6. 缓冲输出模式（可选）

```c
// 启用缓冲输出模式
#define MLOG_BUF_OUTPUT_ENABLE
#define MLOG_BUF_OUTPUT_BUF_SIZE 2048    // 环形缓冲区大小（字节）
```

### 运行时配置 API

#### 设置日志级别

```c
// 设置全局日志级别
mlog_set_filter_lvl(MLOG_LVL_INFO);

// 为特定标签设置级别
mlog_set_filter_tag_lvl("MyModule", MLOG_LVL_DEBUG);

// 获取标签的日志级别
uint8_t level = mlog_get_filter_tag_lvl("MyModule");
```

#### 设置标签过滤

```c
// 只输出指定标签的日志
mlog_set_filter_tag("MyModule");

// 清除标签过滤（输出所有标签）
mlog_set_filter_tag("");
```

#### 设置输出格式

```c
// 为 INFO 级别设置输出格式
// 包含级别、标签、时间、函数名和行号
mlog_set_fmt(MLOG_LVL_INFO,
             MLOG_FMT_LVL | MLOG_FMT_TAG | MLOG_FMT_TIME |
             MLOG_FMT_FUNC | MLOG_FMT_LINE);

// 使用所有格式
mlog_set_fmt(MLOG_LVL_DEBUG, MLOG_FMT_ALL);
```

#### 颜色输出控制

```c
// 启用彩色输出
mlog_set_text_color_enabled(true);

// 禁用彩色输出
mlog_set_text_color_enabled(false);

// 获取颜色输出状态
bool enabled = mlog_get_text_color_enabled();
```

## API 使用示例

### 基本日志输出

```c
#include <mlog.h>

void example_basic_logging(void)
{
    // 6 个日志级别的使用
    mlog_a("tag", "Assert: %s", "critical failure");
    mlog_e("tag", "Error: code = %d", -1);
    mlog_w("tag", "Warning: %s", "low battery");
    mlog_i("tag", "Info: system started");
    mlog_d("tag", "Debug: counter = %d", 100);
    mlog_v("tag", "Verbose: detailed trace");
}
```

### 使用简化宏

```c
#define LOG_TAG "Sensor"
#define LOG_LVL MLOG_LVL_VERBOSE
#include <mlog.h>

void read_sensor(void)
{
    int temperature = 25;

    log_i("Reading sensor...");
    log_d("Temperature: %d°C", temperature);

    if (temperature > 80) {
        log_w("Temperature too high!");
    }
}
```

### 原始输出（无格式）

```c
void example_raw_output(void)
{
    // 输出不带任何格式的原始字符串
    mlog_raw("Raw output: no timestamp, no tag\n");
    mlog_raw("Value: %d\n", 42);
}
```

### 十六进制转储

```c
void example_hexdump(void)
{
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                      0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    // 以 16 字节宽度输出十六进制数据
    mlog_hexdump("DataBuffer", 16, data, sizeof(data));
}
```

输出示例：

```txt
D/HEX DataBuffer: 0000-000F: 01 02 03 04 05 06 07 08  09 0A 0B 0C 0D 0E 0F 10  ................
```

### 断言功能

```c
void example_assert(void)
{
    int value = 10;

    // 断言检查，如果条件为 false，触发断言失败处理
    MLOG_ASSERT(value > 0);
    MLOG_ASSERT(value < 100);
}
```

默认不会定义标准名称 `assert`，避免污染全局命名空间。若确实需要 `assert(expression)` 作为 `MLOG_ASSERT(expression)` 的别名，可在包含 `mlog.h` 前定义：

```c
#define MLOG_USING_ASSERT_ALIAS
#include <mlog.h>
```

### 自定义断言钩子

```c
void my_assert_hook(const char* expr, const char* func, size_t line)
{
    // 自定义断言处理：记录日志、触发复位等
    mlog_e("ASSERT", "Failed at %s:%zu - %s", func, line, expr);

    // 可以在这里添加其他处理，如系统复位
    // system_reset();
}

void setup_assert_hook(void)
{
    mlog_assert_set_hook(my_assert_hook);
}
```

### 过滤器使用示例

```c
void example_filters(void)
{
    // 场景 1：只输出 ERROR 及以上级别的日志
    mlog_set_filter_lvl(MLOG_LVL_ERROR);
    mlog_i("test", "This won't be displayed");  // 不会输出
    mlog_e("test", "This will be displayed");   // 会输出

    // 场景 2：只输出特定标签的日志
    mlog_set_filter_lvl(MLOG_LVL_VERBOSE);  // 恢复所有级别
    mlog_set_filter_tag("network");
    mlog_i("network", "Network event");     // 会输出
    mlog_i("sensor", "Sensor data");        // 不会输出

    // 场景 3：为不同标签设置不同级别
    mlog_set_filter_tag("");  // 清除标签过滤
    mlog_set_filter_tag_lvl("network", MLOG_LVL_INFO);
    mlog_set_filter_tag_lvl("sensor", MLOG_LVL_DEBUG);

    mlog_d("network", "Network debug");     // 不会输出
    mlog_i("network", "Network info");      // 会输出
    mlog_d("sensor", "Sensor debug");       // 会输出
}
```

### 缓冲输出模式

```c
void example_buffered_output(void)
{
    // 启用缓冲输出（需要在 mlog_cfg.h 中定义 MLOG_BUF_OUTPUT_ENABLE）
    mlog_buf_enabled(true);

    // 高频率日志输出会先进入 FIFO 环形缓冲
    for (int i = 0; i < 100; i++) {
        mlog_d("loop", "Iteration %d", i);
    }

    // 手动刷新缓冲区，立即输出所有缓冲的日志
    mlog_flush();

    // 禁用缓冲输出
    mlog_buf_enabled(false);
}
```

### 线程安全示例

```c
// 在移植层实现中提供互斥锁
void example_thread_safe_logging(void)
{
    // MLog 内部使用 output_lock/unlock 保证线程安全
    // 多个线程可以同时调用日志函数
    mlog_i("thread1", "Message from thread 1");
    mlog_i("thread2", "Message from thread 2");

    // 如果需要手动控制锁（高级用法）
    mlog_output_lock();
    mlog_raw("Critical section: ");
    mlog_raw("multiple calls are atomic\n");
    mlog_output_unlock();
}
```

## 移植指南

MLog 通过移植层接口实现平台无关性。您需要在 `port/mlog_port.c` 中实现以下接口。

### 移植层接口说明

移植层接口定义在 `MlogPortInterface` 结构体中：

```c
typedef struct
{
    mlog_port_init_fn_t          init;           // 初始化函数
    mlog_port_deinit_fn_t        deinit;         // 反初始化函数
    mlog_port_output_fn_t        output;         // 日志输出函数，返回前必须消费或复制 log
    mlog_port_output_lock_fn_t   output_lock;    // 输出锁定函数
    mlog_port_output_unlock_fn_t output_unlock;  // 输出解锁函数
    mlog_port_get_time_fn_t      get_time;       // 获取时间戳函数
    mlog_port_get_p_info_fn_t    get_p_info;     // 获取进程信息函数
    mlog_port_get_t_info_fn_t    get_t_info;     // 获取线程信息函数
} MlogPortInterface;
```

`output(log, size)` 的实现必须在返回前完成对 `log` 数据的消费，或者复制到移植层自己的缓冲区。`log` 指向日志库内部临时缓冲区，使用 DMA、RTOS 队列、后台任务等异步输出时，不能保存该指针后延迟使用。阻塞式输出数据生命周期安全，但如果 `output_lock()` 通过关中断保护日志缓冲区，需要评估输出耗时对中断延迟的影响。

### 必须实现的接口

#### 1. 初始化函数（init）

```c
static MlogErrCode default_port_init(void)
{
    // 初始化输出设备（如 UART）
    // uart_init(115200);

    // 初始化互斥锁（如果使用 RTOS）
    // mutex_create(&log_mutex);

    return MLOG_NO_ERR;
}
```

#### 2. 输出函数（output）- **最重要**

```c
static void default_port_output(const char* log, size_t size)
{
    // 示例 1：输出到 UART
    // uart_send(log, size);

    // 示例 2：输出到控制台（PC 平台）
    // fwrite(log, 1, size, stdout);

    // 示例 3：输出到 RTT（J-Link）
    // SEGGER_RTT_Write(0, log, size);

    // 示例 4：输出到自定义缓冲区
    // custom_buffer_write(log, size);

    // 若改为 DMA/队列等异步输出，必须先复制 log 内容。
}
```

缓冲区满时不会阻塞当前日志调用，超出容量的新数据会被丢弃。可通过 `mlog_buf_get_overflow_stats()` 获取丢弃次数和字节数，并用 `mlog_buf_reset_overflow_stats()` 清零统计。

### 可选实现的接口

#### 3. 反初始化函数（deinit）

```c
static void default_port_deinit(void)
{
    // 释放资源
    // mutex_delete(&log_mutex);
}
```

#### 4. 线程安全锁（output_lock / output_unlock）

```c
static void default_port_output_lock(void)
{
    // 裸机系统：关闭中断
    // __disable_irq();

    // RTOS 系统：获取互斥锁
    // mutex_lock(&log_mutex);
}

static void default_port_output_unlock(void)
{
    // 裸机系统：开启中断
    // __enable_irq();

    // RTOS 系统：释放互斥锁
    // mutex_unlock(&log_mutex);
}
```

#### 5. 时间戳函数（get_time）

```c
static const char* default_port_get_time(void)
{
    static char time_str[16];

    // 示例 1：使用系统滴答计数
    // uint32_t tick = HAL_GetTick();
    // snprintf(time_str, sizeof(time_str), "%lu", tick);

    // 示例 2：使用 RTC
    // rtc_get_time_string(time_str, sizeof(time_str));

    // 示例 3：返回固定字符串或空字符串
    return "";
}
```

#### 6. 进程/线程信息函数（get_p_info / get_t_info）

```c
static const char* default_port_get_p_info(void)
{
    // 返回进程 ID 或名称
    // sprintf(buf, "pid:%d", getpid());
    return "";
}

static const char* default_port_get_t_info(void)
{
    static char thread_str[16];

    // 示例：返回 RTOS 任务名称
    // const char* task_name = osThreadGetName(osThreadGetId());
    // snprintf(thread_str, sizeof(thread_str), "%s", task_name);

    return "";
}
```

### 注册移植层接口

在使用 MLog 之前，必须注册移植层接口：

```c
// 方法 1：使用默认接口
mlog_port_register(mlog_port_get_default());

// 方法 2：使用自定义接口
MlogPortInterface my_port = {
    .init = my_init,
    .output = my_output,
    .output_lock = my_lock,
    .output_unlock = my_unlock,
    // ... 其他接口
};
mlog_port_register(&my_port);

// 然后初始化 MLog
mlog_init();
```

### 平台移植示例

#### STM32 + HAL 库

```c
#include "stm32xxxx_hal.h"

extern UART_HandleTypeDef huart1;

static void stm32_port_output(const char* log, size_t size)
{
    // 阻塞发送：函数返回时 HAL 已经消费完 log 数据。
    HAL_UART_Transmit(&huart1, (uint8_t*)log, size, HAL_MAX_DELAY);
}

static const char* stm32_port_get_time(void)
{
    static char time_str[16];
    uint32_t tick = HAL_GetTick();
    snprintf(time_str, sizeof(time_str), "%lu", tick);
    return time_str;
}

static const MlogPortInterface stm32_port = {
    .init = NULL,
    .deinit = NULL,
    .output = stm32_port_output,
    .output_lock = NULL,
    .output_unlock = NULL,
    .get_time = stm32_port_get_time,
    .get_p_info = NULL,
    .get_t_info = NULL,
};

// 在 main 函数中注册
mlog_port_register(&stm32_port);
mlog_init();
mlog_start();
```

#### FreeRTOS

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static SemaphoreHandle_t log_mutex;

static MlogErrCode freertos_port_init(void)
{
    log_mutex = xSemaphoreCreateMutex();
    return (log_mutex != NULL) ? MLOG_NO_ERR : MLOG_ERR_PORT_INIT_FAIL;
}

static void freertos_port_output_lock(void)
{
    xSemaphoreTake(log_mutex, portMAX_DELAY);
}

static void freertos_port_output_unlock(void)
{
    xSemaphoreGive(log_mutex);
}

static const char* freertos_port_get_t_info(void)
{
    return pcTaskGetName(NULL);
}
```

#### Linux / POSIX

```c
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static void linux_port_output(const char* log, size_t size)
{
    fwrite(log, 1, size, stdout);
    fflush(stdout);
}

static void linux_port_output_lock(void)
{
    pthread_mutex_lock(&log_mutex);
}

static void linux_port_output_unlock(void)
{
    pthread_mutex_unlock(&log_mutex);
}

static const char* linux_port_get_time(void)
{
    static char time_str[32];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    snprintf(time_str, sizeof(time_str), "%ld.%06ld",
             tv.tv_sec, tv.tv_usec);
    return time_str;
}
```

### 移植检查清单

- [ ] 实现 `output` 函数，确保日志能正确输出
- [ ] 如果是多线程/多任务环境，实现 `output_lock` 和 `output_unlock`
- [ ] 如果需要时间戳，实现 `get_time` 函数
- [ ] 如果需要任务/线程信息，实现 `get_t_info` 函数
- [ ] 调用 `mlog_port_register()` 注册接口
- [ ] 调用 `mlog_init()` 初始化 MLog
- [ ] 调用 `mlog_start()` 启动日志输出
- [ ] 根据需要调整 `mlog_cfg.h` 中的配置参数

## 许可证

版权所有 (c) 2025 liu (<lbq08@foxmail.com>)

根据源代码文件中的版权声明，本项目的所有权利由作者保留。

```
Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
```

使用本代码前，请联系作者获取授权或查看项目仓库中的许可证文件（如有）。

---

## 贡献与支持

如果您在使用过程中遇到问题或有改进建议，欢迎：

- 提交 Issue：报告 bug 或提出功能请求
- 提交 Pull Request：贡献代码改进

## 相关链接

- GitHub 仓库：<https://github.com/causebefore/mlog-module>
- 作者邮箱：<lbq08@foxmail.com>
