# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

MLog 是一个轻量级嵌入式 C 日志库，专为 Cortex-M 裸机和 RTOS 环境设计。通过移植层抽象硬件差异，支持编译时/运行时双重过滤、缓冲输出和 ANSI 彩色日志。

## 构建命令

```bash
# 生成构建目录 (Windows MSVC / MinGW)
cmake -S . -B build

# 编译静态库 + 示例程序
cmake --build build

# 跳过示例构建
cmake -S . -B build -DMLOG_BUILD_EXAMPLE=OFF
```

没有单元测试框架。`tools/mlog_scan.py` 是静态扫描工具而非运行时测试。

## 架构分层

```
应用层:  mlog_e("tag", ...) / log_e(...)  宏调用
       └── mlog.h 中编译期级别裁剪 (#if MLOG_OUTPUT_LVL >= ...)
内核层:  mlog.c (mlog_output → 格式组装 → 级别/tag 过滤 → 输出)
       └── mlog_buf.c (缓冲模式，可选，#ifdef MLOG_BUF_OUTPUT_ENABLE)
移植层:  mlog_port.c / mlog_port_minimal.c
       └── MlogPortInterface { init, output, lock/unlock, get_time, ... }
```

### 关键文件

| 文件 | 职责 |
|------|------|
| `inc/mlog.h` | 公共 API、宏定义、日志级别枚举、格式选项位掩码 |
| `inc/mlog_cfg.h` | 编译期配置：`MLOG_OUTPUT_LVL`、`MLOG_BUF_OUTPUT_ENABLE`、`MLOG_COLOR_ENABLE`、`MLOG_FMT_USING_*`、缓冲区大小 |
| `src/mlog.c` | 核心实现：`mlog_output()`（格式组装）、`mlog_set_filter_tag_lvl()`（标签级过滤）、`mlog_hexdump()`、断言钩子 |
| `src/mlog_buf.c` | 缓冲输出：满时丢弃新数据不阻塞，支持 `mlog_flush()` / `mlog_flush_partial()` |
| `port/mlog_port.c` | 默认移植层模板（依赖 `shell.h`/`usart.h`，不可跨平台编译） |
| `examples/baremetal_minimal/mlog_port_minimal.c` | 最小移植层示例（使用弱函数 `mlog_port_hw_write`，可直接编译） |
| `tools/mlog_scan.py` | 静态扫描工具（CLI+GUI）：提取所有日志调用、检测 LOG_TAG 缺失、格式串校验 |
| `tools/mlog_serial_logger.py` | 串口日志采集工具：生成 raw.log、jsonl、meta.json，支持 agent 使用 JSON 控制 |

### 调用链 (以 `mlog_e("tag", "msg %d", n)` 为例)

1. `mlog_e` 宏展开为 `mlog_output(MLOG_LVL_ERROR, "tag", dir, func, line, "msg %d", n)`
2. `mlog_output()` 检查 `output_enabled` → 级别过滤 → tag 过滤
3. `fmt_header_level_tag()` 写入级别标识 + tag
4. `fmt_header_context()` 写入 `[时间 进程 线程]`
5. `fmt_header_source()` 写入 `(文件:行号 函数名)`
6. `vsnprintf()` 格式化用户消息到共享缓冲区 `s_log_buf`
7. `fmt_tail()` 追加颜色结束符 + 换行
8. `output_log_line()` → 直接调用 `port->output` 或进入 `mlog_buf_output()`

### 双 API 风格

- **显式 tag**：`mlog_e("MyTag", "msg %d", n)` — tag 在调用处写明
- **隐式 tag**：`log_e("msg %d", n)` — 需文件头定义 `#define LOG_TAG "MyTag"` + `#define LOG_LVL MLOG_LVL_DEBUG`，然后 `#include <mlog.h>`

## 移植层注册 (必须按此顺序)

```c
mlog_port_register(mlog_port_get_default());  // 注册接口
mlog_init();                                   // 初始化 (验证 output 非空)
mlog_start();                                  // 启动输出 (+ 启动缓冲模式)
// ... 使用日志 ...
mlog_stop();
mlog_deinit();
```

`mlog_port_register()` 中 NULL 字段不会被覆盖，仅非 NULL 字段更新。

## 代码风格

使用 `.clang-format` 配置 (clang-format 21, Allman 括号风格, 4 空格缩进, ColumnLimit=120, 指针类型靠左 `int* ptr`, 左对齐宏常量)。

运行格式化：`clang-format -i <文件>` (需要 clang-format 21+)

## 注意事项

- `src/mlog.c` 中 `MLOG_STRCPY` 等宏默认使用 `mlog_strcpy`（项目内实现的带边界保护拷贝），不是标准 `strcpy`
- 配置文件 `mlog_cfg.h` 中的 `MLOG_FILTER_TAG_MAX_LEN` 等被 `#error` 强制检查，缺少会导致编译失败
- `MLOG_ASSERT_ENABLE` 打开时，`MLOG_ASSERT` 失败会调用断言钩子并进入失败动作；只有额外定义 `MLOG_USING_ASSERT_ALIAS` 时才会把 `assert` 别名映射到 `MLOG_ASSERT`
- 缓冲模式 `mlog_flush()` 需要 `output_lock/output/output_unlock` 三个回调全部非空
- `port/mlog_port.c` 有具体平台依赖（`shell.h`, `__get_PRIMASK`），仅用于特定平台；通用示例见 `examples/baremetal_minimal/`
