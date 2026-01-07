# MLog API 文档

本文档详细描述了 MLog 嵌入式日志库的所有 API 接口。

## 目录

- [数据类型](#数据类型)
- [初始化与控制 API](#初始化与控制-api)
- [日志输出 API](#日志输出-api)
- [日志配置 API](#日志配置-api)
- [过滤器 API](#过滤器-api)
- [移植层 API](#移植层-api)
- [实用工具 API](#实用工具-api)
- [宏定义](#宏定义)

---

## 数据类型

### MlogErrCode

错误码枚举类型。

```c
typedef enum
{
    MLOG_ERR_PORT_INIT_FAIL = -1,  // 移植层初始化失败
    MLOG_NO_ERR,                   // 无错误
} MlogErrCode;
```

### MlogLevel

日志级别枚举类型。

```c
typedef enum
{
    MLOG_LVL_ASSERT = 0,  // 断言级别
    MLOG_LVL_ERROR,       // 错误级别
    MLOG_LVL_WARN,        // 警告级别
    MLOG_LVL_INFO,        // 信息级别
    MLOG_LVL_DEBUG,       // 调试级别
    MLOG_LVL_VERBOSE,     // 详细级别
} MlogLevel;
```

**级别说明：**
- `MLOG_LVL_ASSERT`：最高优先级，用于断言失败
- `MLOG_LVL_ERROR`：严重错误，需要立即处理
- `MLOG_LVL_WARN`：警告信息
- `MLOG_LVL_INFO`：重要的运行信息
- `MLOG_LVL_DEBUG`：调试信息
- `MLOG_LVL_VERBOSE`：最详细的日志信息

### MlogFmtIndex

日志格式选项枚举类型。

```c
typedef enum
{
    MLOG_FMT_LVL    = 1 << 0,  // 级别标识
    MLOG_FMT_TAG    = 1 << 1,  // 标签
    MLOG_FMT_TIME   = 1 << 2,  // 时间戳
    MLOG_FMT_P_INFO = 1 << 3,  // 进程信息
    MLOG_FMT_T_INFO = 1 << 4,  // 线程信息
    MLOG_FMT_DIR    = 1 << 5,  // 文件路径
    MLOG_FMT_FUNC   = 1 << 6,  // 函数名
    MLOG_FMT_LINE   = 1 << 7,  // 行号
} MlogFmtIndex;
```

可以通过位或操作组合多个格式选项。

### MlogPortInterface

移植层接口结构体。

```c
typedef struct
{
    mlog_port_init_fn_t          init;           // 初始化回调
    mlog_port_deinit_fn_t        deinit;         // 反初始化回调
    mlog_port_output_fn_t        output;         // 输出回调
    mlog_port_output_lock_fn_t   output_lock;    // 输出锁定回调
    mlog_port_output_unlock_fn_t output_unlock;  // 输出解锁回调
    mlog_port_get_time_fn_t      get_time;       // 获取时间回调
    mlog_port_get_p_info_fn_t    get_p_info;     // 获取进程信息回调
    mlog_port_get_t_info_fn_t    get_t_info;     // 获取线程信息回调
} MlogPortInterface;
```

---

## 初始化与控制 API

### mlog_init

```c
MlogErrCode mlog_init(void);
```

**功能：** 初始化 MLog 日志系统。

**参数：** 无

**返回值：**
- `MLOG_NO_ERR`：初始化成功
- `MLOG_ERR_PORT_INIT_FAIL`：移植层初始化失败

**说明：**
- 必须在使用任何其他 MLog 功能之前调用
- 会调用移植层的初始化函数
- 设置默认的日志级别为 `MLOG_LVL_VERBOSE`
- 如果已经初始化，再次调用不会有任何效果

**示例：**
```c
MlogErrCode result = mlog_init();
if (result != MLOG_NO_ERR) {
    // 初始化失败处理
}
```

### mlog_deinit

```c
void mlog_deinit(void);
```

**功能：** 反初始化 MLog 日志系统，释放资源。

**参数：** 无

**返回值：** 无

**说明：**
- 调用移植层的反初始化函数
- 如果未初始化，调用不会有任何效果

**示例：**
```c
mlog_deinit();
```

### mlog_start

```c
void mlog_start(void);
```

**功能：** 启动日志输出。

**参数：** 无

**返回值：** 无

**说明：**
- 必须在 `mlog_init()` 之后调用
- 启用日志输出功能
- 如果配置了缓冲输出，也会启用缓冲

**示例：**
```c
mlog_init();
mlog_start();
```

### mlog_stop

```c
void mlog_stop(void);
```

**功能：** 停止日志输出。

**参数：** 无

**返回值：** 无

**说明：**
- 禁用日志输出
- 如果配置了缓冲输出，也会禁用缓冲

**示例：**
```c
mlog_stop();
```

---

## 日志输出 API

### mlog_raw_output

```c
void mlog_raw_output(const char* format, ...);
```

**功能：** 输出原始格式的日志，不添加任何格式信息。

**参数：**
- `format`：格式化字符串（类似 printf）
- `...`：可变参数

**返回值：** 无

**说明：**
- 直接输出，不添加级别、标签、时间等信息
- 不受日志级别和过滤器影响

**示例：**
```c
mlog_raw_output("Raw log: %d\n", value);
```

### mlog_output

```c
void mlog_output(uint8_t level, const char* tag, const char* file, 
                 const char* func, const long line, const char* format, ...);
```

**功能：** 输出格式化的日志。

**参数：**
- `level`：日志级别
- `tag`：日志标签
- `file`：文件名（可为 NULL）
- `func`：函数名（可为 NULL）
- `line`：行号
- `format`：格式化字符串
- `...`：可变参数

**返回值：** 无

**说明：**
- 根据配置的格式选项输出日志
- 受日志级别和过滤器控制
- 通常不直接调用，而是通过宏使用

### mlog_assert / mlog_a

```c
mlog_assert(tag, format, ...);
mlog_a(tag, format, ...);
```

**功能：** 输出断言级别日志。

**参数：**
- `tag`：日志标签
- `format`：格式化字符串
- `...`：可变参数

**示例：**
```c
mlog_a("system", "Critical error: %s", error_msg);
```

### mlog_error / mlog_e

```c
mlog_error(tag, format, ...);
mlog_e(tag, format, ...);
```

**功能：** 输出错误级别日志。

**参数：**
- `tag`：日志标签
- `format`：格式化字符串
- `...`：可变参数

**示例：**
```c
mlog_e("network", "Connection failed: %d", error_code);
```

### mlog_warn / mlog_w

```c
mlog_warn(tag, format, ...);
mlog_w(tag, format, ...);
```

**功能：** 输出警告级别日志。

**参数：**
- `tag`：日志标签
- `format`：格式化字符串
- `...`：可变参数

**示例：**
```c
mlog_w("sensor", "Temperature high: %d°C", temp);
```

### mlog_info / mlog_i

```c
mlog_info(tag, format, ...);
mlog_i(tag, format, ...);
```

**功能：** 输出信息级别日志。

**参数：**
- `tag`：日志标签
- `format`：格式化字符串
- `...`：可变参数

**示例：**
```c
mlog_i("main", "System started successfully");
```

### mlog_debug / mlog_d

```c
mlog_debug(tag, format, ...);
mlog_d(tag, format, ...);
```

**功能：** 输出调试级别日志。

**参数：**
- `tag`：日志标签
- `format`：格式化字符串
- `...`：可变参数

**示例：**
```c
mlog_d("parser", "Parsing data: %s", data);
```

### mlog_verbose / mlog_v

```c
mlog_verbose(tag, format, ...);
mlog_v(tag, format, ...);
```

**功能：** 输出详细级别日志。

**参数：**
- `tag`：日志标签
- `format`：格式化字符串
- `...`：可变参数

**示例：**
```c
mlog_v("trace", "Function call: %s", __func__);
```

---

## 日志配置 API

### mlog_set_fmt

```c
void mlog_set_fmt(uint8_t level, size_t set);
```

**功能：** 设置指定日志级别的输出格式。

**参数：**
- `level`：日志级别（`MLOG_LVL_ASSERT` 到 `MLOG_LVL_VERBOSE`）
- `set`：格式选项的位或组合

**返回值：** 无

**说明：**
- 可以为不同的日志级别设置不同的格式
- 使用 `MLOG_FMT_ALL` 启用所有格式选项

**示例：**
```c
// 为 INFO 级别设置格式：级别 + 标签 + 时间
mlog_set_fmt(MLOG_LVL_INFO, MLOG_FMT_LVL | MLOG_FMT_TAG | MLOG_FMT_TIME);

// 为 DEBUG 级别启用所有格式
mlog_set_fmt(MLOG_LVL_DEBUG, MLOG_FMT_ALL);

// 为 ERROR 级别设置：级别 + 标签 + 文件 + 行号
mlog_set_fmt(MLOG_LVL_ERROR, 
             MLOG_FMT_LVL | MLOG_FMT_TAG | MLOG_FMT_DIR | MLOG_FMT_LINE);
```

### mlog_set_text_color_enabled

```c
void mlog_set_text_color_enabled(bool enabled);
```

**功能：** 启用或禁用彩色输出。

**参数：**
- `enabled`：`true` 启用，`false` 禁用

**返回值：** 无

**说明：**
- 需要在编译时定义 `MLOG_COLOR_ENABLE`
- 使用 ANSI 转义序列实现颜色输出

**示例：**
```c
mlog_set_text_color_enabled(true);  // 启用彩色输出
mlog_set_text_color_enabled(false); // 禁用彩色输出
```

### mlog_get_text_color_enabled

```c
bool mlog_get_text_color_enabled(void);
```

**功能：** 获取当前彩色输出状态。

**参数：** 无

**返回值：**
- `true`：彩色输出已启用
- `false`：彩色输出已禁用

**示例：**
```c
if (mlog_get_text_color_enabled()) {
    // 彩色输出已启用
}
```

---

## 过滤器 API

### mlog_set_filter_lvl

```c
void mlog_set_filter_lvl(uint8_t level);
```

**功能：** 设置全局日志级别过滤。

**参数：**
- `level`：日志级别（`MLOG_LVL_ASSERT` 到 `MLOG_LVL_VERBOSE`）

**返回值：** 无

**说明：**
- 只输出级别小于或等于设置级别的日志
- 例如设置为 `MLOG_LVL_INFO`，则只输出 ASSERT、ERROR、WARN、INFO 级别的日志

**示例：**
```c
// 只输出 ERROR 及以上级别的日志
mlog_set_filter_lvl(MLOG_LVL_ERROR);

// 输出所有级别的日志
mlog_set_filter_lvl(MLOG_LVL_VERBOSE);
```

### mlog_set_filter_tag

```c
void mlog_set_filter_tag(const char* tag);
```

**功能：** 设置标签过滤，只输出指定标签的日志。

**参数：**
- `tag`：标签名称，传入空字符串 `""` 或 `NULL` 清除过滤

**返回值：** 无

**说明：**
- 只输出与指定标签完全匹配的日志
- 设置后，其他标签的日志都不会输出

**示例：**
```c
// 只输出 "network" 标签的日志
mlog_set_filter_tag("network");

// 清除标签过滤，输出所有标签
mlog_set_filter_tag("");
```

### mlog_set_filter

```c
void mlog_set_filter(uint8_t level, const char* tag);
```

**功能：** 同时设置级别和标签过滤。

**参数：**
- `level`：日志级别
- `tag`：标签名称

**返回值：** 无

**说明：**
- 等同于依次调用 `mlog_set_filter_lvl()` 和 `mlog_set_filter_tag()`

**示例：**
```c
// 只输出 "sensor" 标签且级别为 INFO 及以上的日志
mlog_set_filter(MLOG_LVL_INFO, "sensor");
```

### mlog_set_filter_tag_lvl

```c
void mlog_set_filter_tag_lvl(const char* tag, uint8_t level);
```

**功能：** 为指定标签设置独立的日志级别。

**参数：**
- `tag`：标签名称
- `level`：日志级别，使用 `MLOG_FILTER_LVL_ALL` 移除该标签的级别过滤

**返回值：** 无

**说明：**
- 可以为不同标签设置不同的日志级别
- 最多支持 `MLOG_FILTER_TAG_LVL_MAX_NUM` 个标签级别过滤
- 设置 `MLOG_FILTER_LVL_SILENT` 可以静默该标签的所有日志

**示例：**
```c
// 为 "network" 标签设置 INFO 级别
mlog_set_filter_tag_lvl("network", MLOG_LVL_INFO);

// 为 "sensor" 标签设置 DEBUG 级别
mlog_set_filter_tag_lvl("sensor", MLOG_LVL_DEBUG);

// 静默 "test" 标签的所有日志
mlog_set_filter_tag_lvl("test", MLOG_FILTER_LVL_SILENT);

// 移除 "network" 标签的级别过滤
mlog_set_filter_tag_lvl("network", MLOG_FILTER_LVL_ALL);
```

### mlog_get_filter_tag_lvl

```c
uint8_t mlog_get_filter_tag_lvl(const char* tag);
```

**功能：** 获取指定标签的日志级别过滤设置。

**参数：**
- `tag`：标签名称

**返回值：**
- 该标签的日志级别，如果未设置则返回 `MLOG_FILTER_LVL_ALL`

**示例：**
```c
uint8_t level = mlog_get_filter_tag_lvl("network");
if (level == MLOG_FILTER_LVL_SILENT) {
    // 该标签已被静默
}
```

---

## 移植层 API

### mlog_port_register

```c
void mlog_port_register(const MlogPortInterface* iface);
```

**功能：** 注册移植层接口。

**参数：**
- `iface`：指向移植层接口结构体的指针

**返回值：** 无

**说明：**
- 必须在 `mlog_init()` 之前调用
- `iface` 中的 `NULL` 字段会被忽略
- 通常使用 `mlog_port_get_default()` 获取默认接口

**示例：**
```c
// 注册默认移植层接口
mlog_port_register(mlog_port_get_default());

// 或者注册自定义接口
MlogPortInterface my_port = {
    .init = my_init,
    .output = my_output,
    // ... 其他回调
};
mlog_port_register(&my_port);
```

### mlog_port_get_default

```c
const MlogPortInterface* mlog_port_get_default(void);
```

**功能：** 获取默认的移植层接口。

**参数：** 无

**返回值：** 指向默认移植层接口的指针

**说明：**
- 返回在 `port/mlog_port.c` 中定义的默认接口
- 用户需要在该文件中实现平台相关的回调函数

**示例：**
```c
mlog_port_register(mlog_port_get_default());
```

### mlog_output_lock

```c
void mlog_output_lock(void);
```

**功能：** 手动锁定日志输出。

**参数：** 无

**返回值：** 无

**说明：**
- 用于多线程环境下保护日志输出
- 必须与 `mlog_output_unlock()` 成对使用
- 通常不需要手动调用，MLog 内部会自动处理

**示例：**
```c
mlog_output_lock();
// 执行需要保护的操作
mlog_raw("Part 1");
mlog_raw("Part 2");
mlog_output_unlock();
```

### mlog_output_unlock

```c
void mlog_output_unlock(void);
```

**功能：** 手动解锁日志输出。

**参数：** 无

**返回值：** 无

**说明：**
- 与 `mlog_output_lock()` 成对使用

---

## 实用工具 API

### mlog_hexdump

```c
void mlog_hexdump(const char* name, uint8_t width, const void* buf, uint16_t size);
```

**功能：** 以十六进制格式转储数据。

**参数：**
- `name`：数据名称（用作日志标签）
- `width`：每行显示的字节数（通常为 16 或 32）
- `buf`：指向数据缓冲区的指针
- `size`：数据大小（字节数）

**返回值：** 无

**说明：**
- 以 DEBUG 级别输出
- 同时显示十六进制值和 ASCII 字符
- 输出格式：`D/HEX name: 地址范围: 十六进制数据  ASCII字符`

**示例：**
```c
uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
mlog_hexdump("MyData", 16, data, sizeof(data));

// 输出类似：
// D/HEX MyData: 0000-0007: 01 02 03 04 05 06 07 08  ........
```

### mlog_assert_set_hook

```c
void mlog_assert_set_hook(void (*hook)(const char* expr, const char* func, size_t line));
```

**功能：** 设置断言失败时的钩子函数。

**参数：**
- `hook`：钩子函数指针，参数为：
  - `expr`：断言表达式字符串
  - `func`：函数名
  - `line`：行号

**返回值：** 无

**说明：**
- 当 `MLOG_ASSERT()` 失败时会调用该钩子
- 可以在钩子中实现自定义的断言处理（如系统复位）

**示例：**
```c
void my_assert_hook(const char* expr, const char* func, size_t line)
{
    mlog_e("ASSERT", "Failed at %s:%zu - %s", func, line, expr);
    // 可以在这里添加系统复位等操作
    system_reset();
}

// 注册钩子
mlog_assert_set_hook(my_assert_hook);
```

### mlog_buf_enabled

```c
void mlog_buf_enabled(bool enabled);
```

**功能：** 启用或禁用缓冲输出模式。

**参数：**
- `enabled`：`true` 启用，`false` 禁用

**返回值：** 无

**说明：**
- 需要在编译时定义 `MLOG_BUF_OUTPUT_ENABLE`
- 启用后，日志会先写入缓冲区，减少输出设备调用
- 适合高频率日志输出场景

**示例：**
```c
mlog_buf_enabled(true);  // 启用缓冲输出
// ... 高频率日志输出
mlog_flush();            // 刷新缓冲区
mlog_buf_enabled(false); // 禁用缓冲输出
```

### mlog_flush

```c
void mlog_flush(void);
```

**功能：** 刷新日志缓冲区，立即输出所有缓冲的日志。

**参数：** 无

**返回值：** 无

**说明：**
- 仅在启用缓冲输出模式时有效
- 将缓冲区中的所有日志立即输出到设备

**示例：**
```c
mlog_flush();
```

---

## 宏定义

### 简化 API 宏

使用前需要定义 `LOG_TAG` 和 `LOG_LVL`：

```c
#define LOG_TAG "MyModule"
#define LOG_LVL MLOG_LVL_DEBUG
#include <mlog.h>
```

然后可以使用以下简化宏：

| 宏 | 等价于 | 说明 |
|---|---|---|
| `log_a(...)` / `loga(...)` | `mlog_a(LOG_TAG, ...)` | 断言级别日志 |
| `log_e(...)` / `loge(...)` | `mlog_e(LOG_TAG, ...)` | 错误级别日志 |
| `log_w(...)` / `logw(...)` | `mlog_w(LOG_TAG, ...)` | 警告级别日志 |
| `log_i(...)` / `logi(...)` | `mlog_i(LOG_TAG, ...)` | 信息级别日志 |
| `log_d(...)` / `logd(...)` | `mlog_d(LOG_TAG, ...)` | 调试级别日志 |
| `log_v(...)` / `logv(...)` | `mlog_v(LOG_TAG, ...)` | 详细级别日志 |

**示例：**
```c
#define LOG_TAG "Sensor"
#define LOG_LVL MLOG_LVL_DEBUG
#include <mlog.h>

void read_sensor(void)
{
    log_i("Reading sensor data...");
    int temp = get_temperature();
    log_d("Temperature: %d°C", temp);
    
    if (temp > 80) {
        log_w("Temperature too high!");
    }
}
```

### 断言宏

```c
MLOG_ASSERT(expression)
```

**功能：** 断言检查，失败时输出错误日志。

**参数：**
- `expression`：要检查的表达式

**说明：**
- 需要在编译时定义 `MLOG_ASSERT_ENABLE`
- 如果表达式为假，会调用断言钩子或输出断言日志

**示例：**
```c
MLOG_ASSERT(ptr != NULL);
MLOG_ASSERT(value > 0 && value < 100);
```

也可以使用简化形式：

```c
assert(expression)  // 等同于 MLOG_ASSERT
```

### 格式选项宏

```c
#define MLOG_FMT_ALL  // 包含所有格式选项
```

**说明：**
- `MLOG_FMT_ALL` 等于所有格式选项的位或组合
- 可用于 `mlog_set_fmt()` 函数

### 过滤器级别宏

```c
#define MLOG_FILTER_LVL_SILENT  // 静默级别（等于 MLOG_LVL_ASSERT）
#define MLOG_FILTER_LVL_ALL     // 所有级别（等于 MLOG_LVL_VERBOSE）
```

**说明：**
- `MLOG_FILTER_LVL_SILENT`：用于静默某个标签的所有日志
- `MLOG_FILTER_LVL_ALL`：用于移除标签的级别过滤

---

## 使用流程

### 基本使用流程

```c
// 1. 注册移植层接口
mlog_port_register(mlog_port_get_default());

// 2. 初始化 MLog
MlogErrCode result = mlog_init();
if (result != MLOG_NO_ERR) {
    // 处理初始化错误
    return;
}

// 3. 启动日志输出
mlog_start();

// 4. 配置日志（可选）
mlog_set_filter_lvl(MLOG_LVL_INFO);
mlog_set_fmt(MLOG_LVL_DEBUG, MLOG_FMT_ALL);

// 5. 使用日志
mlog_i("main", "Application started");
mlog_d("main", "Debug info: %d", value);

// 6. 停止和清理（可选）
mlog_stop();
mlog_deinit();
```

### 高级使用示例

```c
// 为不同模块设置不同的日志级别
mlog_set_filter_tag_lvl("network", MLOG_LVL_INFO);
mlog_set_filter_tag_lvl("sensor", MLOG_LVL_DEBUG);
mlog_set_filter_tag_lvl("display", MLOG_LVL_WARN);

// 使用自定义断言处理
void custom_assert_handler(const char* expr, const char* func, size_t line)
{
    mlog_e("ASSERT", "Assertion failed: %s at %s:%zu", expr, func, line);
    // 触发系统复位或进入错误处理模式
}
mlog_assert_set_hook(custom_assert_handler);

// 使用缓冲输出模式
mlog_buf_enabled(true);
for (int i = 0; i < 1000; i++) {
    mlog_d("loop", "Iteration %d", i);
}
mlog_flush();  // 立即输出所有缓冲的日志
mlog_buf_enabled(false);

// 十六进制数据转储
uint8_t packet[32];
receive_packet(packet, sizeof(packet));
mlog_hexdump("RxPacket", 16, packet, sizeof(packet));
```

---

## 注意事项

1. **初始化顺序**：
   - 必须先调用 `mlog_port_register()`
   - 然后调用 `mlog_init()`
   - 最后调用 `mlog_start()`

2. **线程安全**：
   - 如果在多线程环境使用，必须在移植层实现 `output_lock` 和 `output_unlock`

3. **内存使用**：
   - 每行日志的缓冲区大小由 `MLOG_LINE_BUF_SIZE` 定义
   - 标签最大长度由 `MLOG_FILTER_TAG_MAX_LEN` 定义
   - 注意不要超过这些限制

4. **性能优化**：
   - 在高频率日志场景下，使用缓冲输出模式
   - 通过编译时设置 `MLOG_OUTPUT_LVL` 可以完全移除低优先级的日志代码

5. **编译配置**：
   - 所有配置选项在 `mlog_cfg.h` 中定义
   - 修改配置后需要重新编译

---

**版本信息**
- 文档版本：1.0
- 最后更新：2026-01-07
- 作者：liu (lbq08@foxmail.com)
