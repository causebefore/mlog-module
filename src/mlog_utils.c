/*
 * @Author: liu
 * @Date: 2025-12-09 14:59:19
 * @LastEditors: liu lbq08@foxmail.com
 * @LastEditTime: 2025-12-09 16:14:14
 * @FilePath: \Mlog\src\mlog_utils.c
 * @Description: mlog的工具函数代码文件，实现字符串和内存操作功能
 *
 * Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 */

#include <mlog.h>
#include <string.h>

/**
 * 实现字符串拷贝功能，返回实际拷贝的字符数
 */
size_t mlog_strcpy(size_t cur_len, char *dst, const char *src)
{
    const char *src_old = src;

    assert(dst);
    assert(src);

    while (*src != 0)
    {
        /* make sure destination has enough space */
        if (cur_len++ < MLOG_LINE_BUF_SIZE)
        {
            *dst++ = *src++;
        }
        else
        {
            break;
        }
    }
    return src - src_old;
}

/**
 * 实现按行拷贝字符串功能，返回实际拷贝的字符数
 */
size_t mlog_cpyln(char *line, const char *log, size_t len)
{
    size_t newline_len = strlen(MLOG_NEWLINE_SIGN), copy_size = 0;

    assert(line);
    assert(log);

    while (len--)
    {
        *line++ = *log++;
        copy_size++;
        if (copy_size >= newline_len && !strncmp(log - newline_len, MLOG_NEWLINE_SIGN, newline_len))
        {
            break;
        }
    }
    return copy_size;
}

/**
 * 实现内存拷贝功能，返回目标地址
 */
void *mlog_memcpy(void *dst, const void *src, size_t count)
{
    char *tmp = (char *)dst, *s = (char *)src;

    assert(dst);
    assert(src);

    while (count--)
        *tmp++ = *s++;

    return dst;
}

/**
 * 实现内存设置功能，返回目标地址
 */
void *mlog_memset(void *dst, int val, size_t count)
{
    char *tmp = (char *)dst;

    assert(dst);

    while (count--)
        *tmp++ = (char)val;

    return dst;
}
