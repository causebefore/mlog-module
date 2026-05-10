/*
 * @Author: liu
 * @Date: 2025-12-09 14:59:19
 * @LastEditors: liu lbq08@foxmail.com
 * @LastEditTime: 2026-01-09 22:40:00
 * @FilePath: \RVMDK（uv5）c:\Users\lbqdl\Desktop\USART1接发\User\Mlog\src\mlog_buf.c
 * @Description:
 * mlog的缓冲区输出模式代码文件，实现日志缓冲区输出功能，适合用于高频率日志输出场景，减少输出设备的调用次数，提高性能。
 * 环形缓冲版本：满时丢弃新数据，永不阻塞
 *
 * Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 */

#include <string.h>

#include <mlog.h>

#ifdef MLOG_BUF_OUTPUT_ENABLE
    #if !defined(MLOG_BUF_OUTPUT_BUF_SIZE)
        #error "Please configure buffer size for buffered output mode (in mlog_cfg.h)"
    #endif
    #if MLOG_BUF_OUTPUT_BUF_SIZE == 0
        #error "MLOG_BUF_OUTPUT_BUF_SIZE must be greater than 0"
    #endif
    #if MLOG_BUF_OUTPUT_BUF_SIZE == 1
        #define MLOG_BUF_RING_ADVANCE(pos, size) (0U)
    #else
        #define MLOG_BUF_RING_ADVANCE(pos, size) (((pos) + (size)) % MLOG_BUF_OUTPUT_BUF_SIZE)
    #endif

/* buffered output mode's ring buffer */
static char s_log_buf[MLOG_BUF_OUTPUT_BUF_SIZE];
/* ring buffer read/write state */
static size_t s_buf_read_pos  = 0;
static size_t s_buf_write_pos = 0;
static size_t s_buf_used      = 0;
/* buffered output mode enabled flag */
static bool s_is_enabled = false;
/* overflow statistics */
static uint32_t s_overflow_count = 0; /* 溢出丢弃次数 */
static uint32_t s_overflow_bytes = 0; /* 溢出丢弃字节数 */

static void ring_copy_in(const char* log, size_t size)
{
    size_t first_len = MLOG_BUF_OUTPUT_BUF_SIZE - s_buf_write_pos;
    if (first_len > size)
    {
        first_len = size;
    }

    memcpy(&s_log_buf[s_buf_write_pos], log, first_len);
    if (size > first_len)
    {
        memcpy(s_log_buf, log + first_len, size - first_len);
    }

    s_buf_write_pos = MLOG_BUF_RING_ADVANCE(s_buf_write_pos, size);
    s_buf_used += size;
}

static void ring_discard_read(size_t size)
{
    s_buf_read_pos = MLOG_BUF_RING_ADVANCE(s_buf_read_pos, size);
    s_buf_used -= size;

    if (s_buf_used == 0U)
    {
        s_buf_read_pos  = 0U;
        s_buf_write_pos = 0U;
    }
}

static void ring_output(const MlogPortInterface* iface, size_t size)
{
    size_t first_len = MLOG_BUF_OUTPUT_BUF_SIZE - s_buf_read_pos;
    if (first_len > size)
    {
        first_len = size;
    }

    iface->output(&s_log_buf[s_buf_read_pos], first_len);
    if (size > first_len)
    {
        iface->output(s_log_buf, size - first_len);
    }
}

static void overflow_add(size_t bytes)
{
    s_overflow_count++;
    if (bytes > ((size_t) UINT32_MAX - (size_t) s_overflow_bytes))
    {
        s_overflow_bytes = UINT32_MAX;
    }
    else
    {
        s_overflow_bytes += (uint32_t) bytes;
    }
}

/**
 * @brief  获取溢出统计信息
 * @param  count: 溢出次数（可为NULL）
 * @param  bytes: 溢出字节数（可为NULL）
 */
void mlog_buf_get_overflow_stats(uint32_t* count, uint32_t* bytes)
{
    mlog_output_lock();
    if (count != NULL)
    {
        *count = s_overflow_count;
    }
    if (bytes != NULL)
    {
        *bytes = s_overflow_bytes;
    }
    mlog_output_unlock();
}

/**
 * @brief  重置溢出统计
 */
void mlog_buf_reset_overflow_stats(void)
{
    mlog_output_lock();
    s_overflow_count = 0;
    s_overflow_bytes = 0;
    mlog_output_unlock();
}

/**
 * @brief  获取缓冲区使用情况
 * @return 当前已使用字节数
 */
size_t mlog_buf_get_used(void)
{
    mlog_output_lock();
    size_t used = s_buf_used;
    mlog_output_unlock();
    return used;
}

/**
 * @brief  获取缓冲区剩余空间
 * @return 剩余可用字节数
 */
size_t mlog_buf_get_free(void)
{
    mlog_output_lock();
    size_t free_space = MLOG_BUF_OUTPUT_BUF_SIZE - s_buf_used;
    mlog_output_unlock();
    return free_space;
}

/**
 * output buffered logs when buffer is full
 * 环形缓冲区满时丢弃新数据，永不阻塞
 *
 * @param log will be buffered line's log
 * @param size log size
 */
void mlog_buf_output(const char* log, size_t size)
{
    const MlogPortInterface* iface = mlog_get_port_interface();
    if ((log == NULL) || (size == 0U))
    {
        return;
    }

    if (!s_is_enabled)
    {
        /* 未启用缓冲模式，直接输出 */
        if (iface->output != NULL)
        {
            iface->output(log, size);
        }
        return;
    }

    /* 计算可用空间 */
    size_t free_space = MLOG_BUF_OUTPUT_BUF_SIZE - s_buf_used;
    size_t write_size = size;

    if (write_size > free_space)
    {
        write_size = free_space;
        overflow_add(size - write_size);
    }

    if (write_size > 0U)
    {
        ring_copy_in(log, write_size);
    }
}

/**
 * flush all buffered logs to output device
 * 优化：移除无用的memset
 */
void mlog_flush(void)
{
    const MlogPortInterface* iface = mlog_get_port_interface();

    /* P3: 接口函数判空保护 */
    if (iface->output_lock == NULL || iface->output == NULL || iface->output_unlock == NULL)
    {
        return;
    }

    /* lock output */
    iface->output_lock();

    if (s_buf_used == 0U)
    {
        iface->output_unlock();
        return;
    }

    /* output log */
    ring_output(iface, s_buf_used);
    s_buf_read_pos  = 0U;
    s_buf_write_pos = 0U;
    s_buf_used      = 0U;

    /* unlock output */
    iface->output_unlock();
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

    const MlogPortInterface* iface = mlog_get_port_interface();

    /* P3: 接口函数判空保护 */
    if (iface->output_lock == NULL || iface->output == NULL || iface->output_unlock == NULL)
    {
        return 0;
    }

    /* lock output */
    iface->output_lock();

    if (s_buf_used == 0U)
    {
        iface->output_unlock();
        return 0U;
    }

    /* 计算本次输出量 */
    to_output = s_buf_used;
    if (max_bytes > 0 && to_output > max_bytes)
    {
        to_output = max_bytes;
    }

    /* output log */
    ring_output(iface, to_output);
    ring_discard_read(to_output);

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
    s_is_enabled = enabled;
}
#endif /* MLOG_BUF_OUTPUT_ENABLE */
