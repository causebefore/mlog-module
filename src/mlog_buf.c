/*
 * @Author: liu
 * @Date: 2025-12-09 14:59:19
 * @LastEditors: liu lbq08@foxmail.com
 * @LastEditTime: 2026-01-09 22:40:00
 * @FilePath: \RVMDK（uv5）c:\Users\lbqdl\Desktop\USART1接发\User\Mlog\src\mlog_buf.c
 * @Description:
 * mlog的缓冲区输出模式代码文件，实现日志缓冲区输出功能，适合用于高频率日志输出场景，减少输出设备的调用次数，提高性能。
 * 优化版本：满时丢弃新数据，永不阻塞
 *
 * Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 */

#include <string.h>

#include <mlog.h>

#ifdef MLOG_BUF_OUTPUT_ENABLE
    #if !defined(MLOG_BUF_OUTPUT_BUF_SIZE)
        #error "Please configure buffer size for buffered output mode (in mlog_cfg.h)"
    #endif

/* buffered output mode's buffer */
static char log_buf[MLOG_BUF_OUTPUT_BUF_SIZE];
/* log buffer current write size */
static size_t buf_write_size = 0;
/* buffered output mode enabled flag */
static bool is_enabled = false;
/* overflow statistics */
static uint32_t overflow_count = 0; /* 溢出丢弃次数 */
static uint32_t overflow_bytes = 0; /* 溢出丢弃字节数 */

/**
 * @brief  获取溢出统计信息
 * @param  count: 溢出次数（可为NULL）
 * @param  bytes: 溢出字节数（可为NULL）
 */
void mlog_buf_get_overflow_stats(uint32_t* count, uint32_t* bytes)
{
    if (count != NULL)
    {
        *count = overflow_count;
    }
    if (bytes != NULL)
    {
        *bytes = overflow_bytes;
    }
}

/**
 * @brief  重置溢出统计
 */
void mlog_buf_reset_overflow_stats(void)
{
    overflow_count = 0;
    overflow_bytes = 0;
}

/**
 * @brief  获取缓冲区使用情况
 * @return 当前已使用字节数
 */
size_t mlog_buf_get_used(void)
{
    return buf_write_size;
}

/**
 * @brief  获取缓冲区剩余空间
 * @return 剩余可用字节数
 */
size_t mlog_buf_get_free(void)
{
    return MLOG_BUF_OUTPUT_BUF_SIZE - buf_write_size;
}

/**
 * output buffered logs when buffer is full
 * 优化：缓冲区满时丢弃新数据，永不阻塞
 *
 * @param log will be buffered line's log
 * @param size log size
 */
void mlog_buf_output(const char* log, size_t size)
{
    MlogPortInterface* iface = mlog_get_port_interface();
    if (!is_enabled)
    {
        /* 未启用缓冲模式，直接输出 */
        iface->output(log, size);
        return;
    }

    /* 计算可用空间 */
    size_t free_space = MLOG_BUF_OUTPUT_BUF_SIZE - buf_write_size;

    if (size <= free_space)
    {
        /* 空间足够，全部写入 */
        memcpy(log_buf + buf_write_size, log, size);
        buf_write_size += size;
    }
    else if (free_space > 0)
    {
        /* 空间不足，写入能写的部分，丢弃剩余 */
        memcpy(log_buf + buf_write_size, log, free_space);
        buf_write_size = MLOG_BUF_OUTPUT_BUF_SIZE;
        /* 统计丢弃的数据 */
        overflow_count++;
        overflow_bytes += (size - free_space);
    }
    else
    {
        /* 完全没有空间，全部丢弃 */
        overflow_count++;
        overflow_bytes += size;
    }
}

/**
 * flush all buffered logs to output device
 * 优化：移除无用的memset
 */
void mlog_flush(void)
{
    size_t             to_output;
    MlogPortInterface* iface = mlog_get_port_interface();
    if (buf_write_size == 0)
    {
        return;
    }

    /* lock output */
    iface->output_lock();

    /* 保存当前大小并重置（允许中断期间有新日志写入） */
    to_output      = buf_write_size;
    buf_write_size = 0;

    /* output log */
    iface->output(log_buf, to_output);

    /* unlock output */
    iface->output_unlock();
    /* 不需要memset，下次写入会覆盖 */
}

/**
 * flush buffered logs with size limit (partial flush)
 * 分块刷新，避免长时间占用
 *
 * @param max_bytes 最大输出字节数（0=全部输出）
 * @return 实际输出的字节数
 */
size_t mlog_flush_partial(size_t max_bytes)
{
    size_t to_output;

    if (buf_write_size == 0)
    {
        return 0;
    }
    MlogPortInterface* iface = mlog_get_port_interface();
    /* lock output */
    iface->output_lock();

    /* 计算本次输出量 */
    to_output = buf_write_size;
    if (max_bytes > 0 && to_output > max_bytes)
    {
        to_output = max_bytes;
    }

    /* output log */
    iface->output(log_buf, to_output);

    /* 移动剩余数据到缓冲区头部 */
    if (to_output < buf_write_size)
    {
        size_t remaining = buf_write_size - to_output;
        memmove(log_buf, log_buf + to_output, remaining);
        buf_write_size = remaining;
    }
    else
    {
        buf_write_size = 0;
    }

    /* unlock output */
    iface->output_unlock();

    return to_output;
}

/**
 * enable or disable buffered output mode
 * the log will be output directly when mode is disabled
 *
 * @param enabled true: enabled, false: disabled
 */
void mlog_buf_enabled(bool enabled)
{
    is_enabled = enabled;
}
#endif /* MLOG_BUF_OUTPUT_ENABLE */
