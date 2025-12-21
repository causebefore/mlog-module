/*
 * @Author: liu
 * @Date: 2025-12-09 14:59:19
 * @LastEditors: liu lbq08@foxmail.com
 * @LastEditTime: 2025-12-09 17:32:38
 * @FilePath: \RVMDK（uv5）c:\Users\lbqdl\Desktop\USART1接发\User\Mlog\src\mlog_buf.c
 * @Description: mlog的缓冲区输出模式代码文件，实现日志缓冲区输出功能，适合用于高频率日志输出场景，减少输出设备的调用次数，提高性能。
 *
 * Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 */

#include <mlog.h>
#include <string.h>

#ifdef MLOG_BUF_OUTPUT_ENABLE
#if !defined(MLOG_BUF_OUTPUT_BUF_SIZE)
#error "Please configure buffer size for buffered output mode (in mlog_cfg.h)"
#endif

/* buffered output mode's buffer */
static char log_buf[MLOG_BUF_OUTPUT_BUF_SIZE] = {0};
/* log buffer current write size */
static size_t buf_write_size = 0;
/* buffered output mode enabled flag */
static bool is_enabled = false;

extern void mlog_port_output(const char *log, size_t size);
extern void mlog_output_lock(void);
extern void mlog_output_unlock(void);

/**
 * output buffered logs when buffer is full
 *
 * @param log will be buffered line's log
 * @param size log size
 */
void mlog_buf_output(const char *log, size_t size)
{
    size_t write_size = 0, write_index = 0;

    if (!is_enabled)
    {
        mlog_port_output(log, size);
        return;
    }

    while (true)
    {
        if (buf_write_size + size > MLOG_BUF_OUTPUT_BUF_SIZE)
        {
            write_size = MLOG_BUF_OUTPUT_BUF_SIZE - buf_write_size;
            memcpy(log_buf + buf_write_size, log + write_index, write_size);
            write_index += write_size;
            size -= write_size;
            /* output log */
            mlog_port_output(log_buf, MLOG_BUF_OUTPUT_BUF_SIZE);
            /* reset write index */
            buf_write_size = 0;
        }
        else
        {
            memcpy(log_buf + buf_write_size, log + write_index, size);
            buf_write_size += size;
            break;
        }
    }
}

/**
 * flush all buffered logs to output device
 */
void mlog_flush(void)
{
    if (buf_write_size == 0)
        return;
    /* lock output */
    mlog_output_lock();
    /* output log */
    mlog_port_output(log_buf, buf_write_size);
    /* reset write index */
    buf_write_size = 0;
    /* unlock output */
    mlog_output_unlock();
    mlog_memset(log_buf, 0, sizeof(log_buf));
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
