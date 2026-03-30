/*
 * @Author: liu
 * @Date: 2025-12-09 14:59:19
 * @LastEditors: liu lbq08@foxmail.com
 * @LastEditTime: 2025-12-09 17:34:01
 * @FilePath: \RVMDK（uv5）c:\Users\lbqdl\Desktop\USART1接发\User\Mlog\src\mlog.c
 * @Description: mlog的核心代码文件，实现日志输出功能
 *
 * Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 */

#define LOG_TAG "mlog"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <mlog.h>

/* Static port interface instance - initialized with NULL callbacks */
static MlogPortInterface s_port = {0};

const MlogPortInterface* mlog_get_port_interface(void)
{
    return &s_port;
}

/**
 * Register port interface callbacks
 * Call this before mlog_init() to set up hardware-specific implementations
 *
 * @param iface pointer to interface structure (NULL fields are ignored)
 */
void mlog_port_register(const MlogPortInterface* iface)
{
    if (iface == NULL)
    {
        return;
    }

    if (iface->init != NULL)
    {
        s_port.init = iface->init;
    }
    if (iface->deinit != NULL)
    {
        s_port.deinit = iface->deinit;
    }
    if (iface->output != NULL)
    {
        s_port.output = iface->output;
    }
    if (iface->output_lock != NULL)
    {
        s_port.output_lock = iface->output_lock;
    }
    if (iface->output_unlock != NULL)
    {
        s_port.output_unlock = iface->output_unlock;
    }
    if (iface->get_time != NULL)
    {
        s_port.get_time = iface->get_time;
    }
    if (iface->get_p_info != NULL)
    {
        s_port.get_p_info = iface->get_p_info;
    }
    if (iface->get_t_info != NULL)
    {
        s_port.get_t_info = iface->get_t_info;
    }
}

#if defined(MLOG_BUF_OUTPUT_ENABLE)
extern void mlog_buf_output(const char* log, size_t size);
#endif

#if !defined(MLOG_OUTPUT_LVL)
    #error "Please configure static output log level (in mlog_cfg.h)"
#endif

#if !defined(MLOG_LINE_NUM_MAX_LEN)
    #error "Please configure output line number max length (in mlog_cfg.h)"
#endif

#if !defined(MLOG_LINE_BUF_SIZE)
    #error "Please configure buffer size for every line's log (in mlog_cfg.h)"
#endif

#if !defined(MLOG_FILTER_TAG_MAX_LEN)
    #error "Please configure output filter's tag max length (in mlog_cfg.h)"
#endif

#if !defined(MLOG_NEWLINE_SIGN)
    #error "Please configure output newline sign (in mlog_cfg.h)"
#endif

/* output filter's tag level max num */
#ifndef MLOG_FILTER_TAG_LVL_MAX_NUM
    #define MLOG_FILTER_TAG_LVL_MAX_NUM 4
#endif

#ifdef MLOG_COLOR_ENABLE
    /**
     * CSI(Control Sequence Introducer/Initiator) sign
     * more information on https://en.wikipedia.org/wiki/ANSI_escape_code
     */
    #define CSI_START "\033["
    #define CSI_END   "\033[0m"
    /* output log front color */
    #define F_BLACK   "30;"
    #define F_RED     "31;"
    #define F_GREEN   "32;"
    #define F_YELLOW  "33;"
    #define F_BLUE    "34;"
    #define F_MAGENTA "35;"
    #define F_CYAN    "36;"
    #define F_WHITE   "37;"
    /* output log background color */
    #define B_NULL
    #define B_BLACK   "40;"
    #define B_RED     "41;"
    #define B_GREEN   "42;"
    #define B_YELLOW  "43;"
    #define B_BLUE    "44;"
    #define B_MAGENTA "45;"
    #define B_CYAN    "46;"
    #define B_WHITE   "47;"
    /* output log fonts style */
    #define S_BOLD      "1m"
    #define S_UNDERLINE "4m"
    #define S_BLINK     "5m"
    #define S_NORMAL    "22m"
    /* output log default color definition: [front color] + [background color] + [show style] */
    #ifndef MLOG_COLOR_ASSERT
        #define MLOG_COLOR_ASSERT (F_MAGENTA B_NULL S_NORMAL)
    #endif
    #ifndef MLOG_COLOR_ERROR
        #define MLOG_COLOR_ERROR (F_RED B_NULL S_NORMAL)
    #endif
    #ifndef MLOG_COLOR_WARN
        #define MLOG_COLOR_WARN (F_YELLOW B_NULL S_NORMAL)
    #endif
    #ifndef MLOG_COLOR_INFO
        #define MLOG_COLOR_INFO (F_CYAN B_NULL S_NORMAL)
    #endif
    #ifndef MLOG_COLOR_DEBUG
        #define MLOG_COLOR_DEBUG (F_GREEN B_NULL S_NORMAL)
    #endif
    #ifndef MLOG_COLOR_VERBOSE
        #define MLOG_COLOR_VERBOSE (F_BLUE B_NULL S_NORMAL)
    #endif
#endif /* MLOG_COLOR_ENABLE */

/* 内部结构体定义（对外隐藏实现细节） */

/* 按标签过滤的级别条目 */
typedef struct
{
    uint8_t level;
    char    tag[MLOG_FILTER_TAG_MAX_LEN + 1];
    bool    tag_use_flag; /**< false: 未使用  true: 已使用 */
} MlogTagLvlFilter;

/* 输出过滤器 */
typedef struct
{
    uint8_t          level;
    char             tag[MLOG_FILTER_TAG_MAX_LEN + 1];
    MlogTagLvlFilter tag_lvl[MLOG_FILTER_TAG_LVL_MAX_NUM];
} MlogFilter;

/* 日志主控结构体 */
typedef struct
{
    MlogFilter filter;
    size_t     enabled_fmt_set[MLOG_LVL_TOTAL_NUM];
    bool       init_ok;
    bool       output_enabled;
#ifdef MLOG_COLOR_ENABLE
    bool text_color_enabled;
#endif
} MLogger;

/* MLogger object */
static MLogger s_mlog;
/* every line log's buffer */
static char s_log_buf[MLOG_LINE_BUF_SIZE] = {0};
/* level output info */
static const char* s_level_output_info[] = {
    [MLOG_LVL_ASSERT]  = "A/",
    [MLOG_LVL_ERROR]   = "E/",
    [MLOG_LVL_WARN]    = "W/",
    [MLOG_LVL_INFO]    = "I/",
    [MLOG_LVL_DEBUG]   = "D/",
    [MLOG_LVL_VERBOSE] = "V/",
};

/* compile-time constants to avoid repeated strlen() calls */
#define NEWLINE_LEN (sizeof(MLOG_NEWLINE_SIGN) - 1)
#ifdef MLOG_COLOR_ENABLE
    #define CSI_START_LEN (sizeof(CSI_START) - 1)
    #define CSI_END_LEN   (sizeof(CSI_END) - 1)
#endif

#ifdef MLOG_COLOR_ENABLE
/* color output info */
static const char* s_color_output_info[] = {
    [MLOG_LVL_ASSERT]  = MLOG_COLOR_ASSERT,
    [MLOG_LVL_ERROR]   = MLOG_COLOR_ERROR,
    [MLOG_LVL_WARN]    = MLOG_COLOR_WARN,
    [MLOG_LVL_INFO]    = MLOG_COLOR_INFO,
    [MLOG_LVL_DEBUG]   = MLOG_COLOR_DEBUG,
    [MLOG_LVL_VERBOSE] = MLOG_COLOR_VERBOSE,
};
#endif /* MLOG_COLOR_ENABLE */

/* Optimized macros (zero function call overhead) */
#define GET_FMT_ENABLED(level, set)       (s_mlog.enabled_fmt_set[level] & (set))
#define GET_FMT_USED_PTR(level, set, arg) ((arg) && GET_FMT_ENABLED(level, set))
#define GET_FMT_USED_INT(level, set, arg) ((arg) && GET_FMT_ENABLED(level, set))

/* Common helper functions */
static inline void set_filter_string(char* dst, const char* src, size_t max_len)
{
    if (src == NULL || src[0] == '\0')
    {
        dst[0] = '\0';
    }
    else
    {
        strncpy(dst, src, max_len);
        dst[max_len] = '\0';
    }
}

static inline void output_log_line(const char* log, size_t len)
{
#if defined(MLOG_BUF_OUTPUT_ENABLE)
    mlog_buf_output(log, len);
#else
    if (s_port.output != NULL)
    {
        s_port.output(log, len);
    }
#endif
}

/* optimized string helpers (bounded copy) */
size_t mlog_strcpy(size_t cur_len, char* dst, const char* src)
{
    const char* src_old = src;

    MLOG_ASSERT(dst != NULL);
    MLOG_ASSERT(src != NULL);

    while (*src != '\0')
    {
        if (cur_len >= MLOG_LINE_BUF_SIZE)
        {
            break;
        }
        *dst++ = *src++;
        cur_len++;
    }
    return (size_t) (src - src_old);
}

static void mlog_set_filter_tag_lvl_default(void);

/* MLogger assert hook */
void (*g_mlog_assert_hook)(const char* expr, const char* func, size_t line);

MlogErrCode mlog_init(void)
{
    MlogErrCode result = MLOG_NO_ERR;

    if (s_mlog.init_ok == true)
    {
        return result;
    }

    /* port initialize */
    if (s_port.init != NULL)
    {
        result = s_port.init();
    }
    if (result != MLOG_NO_ERR)
    {
        return result;
    }

    /* 关键回调验证：output 必须已注册，否则日志无法输出 */
    if (s_port.output == NULL)
    {
        return MLOG_ERR_PORT_INIT_FAIL;
    }

#ifdef MLOG_COLOR_ENABLE
    /* enable text color by default */
    mlog_set_text_color_enabled(true);
#endif

    /* 设置默认格式：全部启用 */
    for (uint8_t i = 0; i < MLOG_LVL_TOTAL_NUM; i++)
    {
        s_mlog.enabled_fmt_set[i] = MLOG_FMT_ALL;
    }

    /* set level is MLOG_LVL_VERBOSE */
    mlog_set_filter_lvl(MLOG_LVL_VERBOSE);

    /* set tag_level to default val */
    mlog_set_filter_tag_lvl_default();

    s_mlog.init_ok = true;
    return result;
}

/**
 * MLogger deinitialize.
 *
 */
void mlog_deinit(void)
{
    if (!s_mlog.init_ok)
    {
        return;
    }

    /* port deinitialize */
    if (s_port.deinit != NULL)
    {
        s_port.deinit();
    }

    s_mlog.init_ok = false;
}

/**
 * set output enable or disable
 *
 * @param enabled TRUE: enable FALSE: disable
 */
static void mlog_set_output_enabled(bool enabled)
{
    MLOG_ASSERT((enabled == false) || (enabled == true));

    s_mlog.output_enabled = enabled;
}

/**
 * MLogger start after initialize.
 */
void mlog_start(void)
{
    if (!s_mlog.init_ok)
    {
        return;
    }

    /* enable output */
    mlog_set_output_enabled(true);

#if defined(MLOG_BUF_OUTPUT_ENABLE)
    mlog_buf_enabled(true);
#endif
}

void mlog_stop(void)
{
    if (!s_mlog.init_ok)
    {
        return;
    }

    /* disable output */
    mlog_set_output_enabled(false);

#if defined(MLOG_BUF_OUTPUT_ENABLE)
    mlog_buf_enabled(false);
#endif
}


#ifdef MLOG_COLOR_ENABLE
/**
 * set log text color enable or disable
 *
 * @param enabled TRUE: enable FALSE:disable
 */
void mlog_set_text_color_enabled(bool enabled)
{
    MLOG_ASSERT((enabled == false) || (enabled == true));

    s_mlog.text_color_enabled = enabled;
}

/**
 * get log text color enable status
 *
 * @return enable or disable
 */
bool mlog_get_text_color_enabled(void)
{
    return s_mlog.text_color_enabled;
}
#endif /* MLOG_COLOR_ENABLE */


/**
 * set log output format. only enable or disable
 *
 * @param level level
 * @param set format set
 */
void mlog_set_fmt(uint8_t level, size_t set)
{
    MLOG_ASSERT(level <= MLOG_LVL_VERBOSE);

    s_mlog.enabled_fmt_set[level] = set;
}

/**
 * set log filter all parameter
 *
 * @param level level
 * @param tag tag
 */
void mlog_set_filter(uint8_t level, const char* tag)
{
    MLOG_ASSERT(level <= MLOG_LVL_VERBOSE);

    mlog_set_filter_lvl(level);
    mlog_set_filter_tag(tag);
}

/**
 * set log filter's level
 */
void mlog_set_filter_lvl(uint8_t level)
{
    MLOG_ASSERT(level <= MLOG_LVL_VERBOSE);
    s_mlog.filter.level = level;
}

/**
 * set log filter's tag
 *
 * @param tag tag (NULL or empty string to clear filter)
 */
void mlog_set_filter_tag(const char* tag)
{
    set_filter_string(s_mlog.filter.tag, tag, MLOG_FILTER_TAG_MAX_LEN);
}

/**
 * lock output
 */
void mlog_output_lock(void)
{
    if (s_port.output_lock != NULL)
    {
        s_port.output_lock();
    }
}

/**
 * unlock output
 */
void mlog_output_unlock(void)
{
    if (s_port.output_unlock != NULL)
    {
        s_port.output_unlock();
    }
}

/**
 * set log filter's tag level val to default
 */
static void mlog_set_filter_tag_lvl_default(void)
{
    /* Optimized: use single memset for the entire structure */
    for (uint8_t i = 0; i < MLOG_FILTER_TAG_LVL_MAX_NUM; i++)
    {
        s_mlog.filter.tag_lvl[i].tag[0]       = '\0';  // just clear first char
        s_mlog.filter.tag_lvl[i].level        = MLOG_FILTER_LVL_SILENT;
        s_mlog.filter.tag_lvl[i].tag_use_flag = false;
    }
}

/**
 * Set the filter's level by different tag.
 * The log on this tag which level is less than it will stop output.
 *
 * example:
 *     // the example tag log enter silent mode
 *     mlog_set_filter_tag_lvl("example", MLOG_FILTER_LVL_SILENT);
 *     // the example tag log which level is less than INFO level will stop output
 *     mlog_set_filter_tag_lvl("example", MLOG_LVL_INFO);
 *     // remove example tag's level filter, all level log will resume output
 *     mlog_set_filter_tag_lvl("example", MLOG_FILTER_LVL_ALL);
 *
 * @param tag log tag
 * @param level The filter level. When the level is MLOG_FILTER_LVL_SILENT, the log enter silent mode.
 *        When the level is MLOG_FILTER_LVL_ALL, it will remove this tag's level filer.
 *        Then all level log will resume output.
 *
 */
void mlog_set_filter_tag_lvl(const char* tag, uint8_t level)
{
    MLOG_ASSERT(level <= MLOG_LVL_VERBOSE);
    MLOG_ASSERT(tag != NULL);

    if (!s_mlog.init_ok || tag[0] == '\0')
    {
        return;
    }

    mlog_output_lock();

    uint8_t i;
    int8_t  found_idx = -1;
    int8_t  empty_idx = -1;

    /* Single pass: find existing tag or first empty slot */
    for (i = 0; i < MLOG_FILTER_TAG_LVL_MAX_NUM; i++)
    {
        if (s_mlog.filter.tag_lvl[i].tag_use_flag)
        {
            if (strncmp(tag, s_mlog.filter.tag_lvl[i].tag, MLOG_FILTER_TAG_MAX_LEN) == 0)
            {
                found_idx = i;
                break;  // early exit
            }
        }
        else if (empty_idx == -1)
        {
            empty_idx = i;  // remember first empty slot
        }
    }

    if (found_idx >= 0)
    {
        /* Tag exists - update or remove */
        if (level == MLOG_FILTER_LVL_ALL)
        {
            /* Remove filter */
            s_mlog.filter.tag_lvl[found_idx].tag_use_flag = false;
            s_mlog.filter.tag_lvl[found_idx].tag[0]       = '\0';
            s_mlog.filter.tag_lvl[found_idx].level        = MLOG_FILTER_LVL_SILENT;
        }
        else
        {
            /* Update level */
            s_mlog.filter.tag_lvl[found_idx].level = level;
        }
    }
    else if (level != MLOG_FILTER_LVL_ALL && empty_idx >= 0)
    {
        /* Add new filter */
        strncpy(s_mlog.filter.tag_lvl[empty_idx].tag, tag, MLOG_FILTER_TAG_MAX_LEN);
        s_mlog.filter.tag_lvl[empty_idx].tag[MLOG_FILTER_TAG_MAX_LEN] = '\0';
        s_mlog.filter.tag_lvl[empty_idx].level                        = level;
        s_mlog.filter.tag_lvl[empty_idx].tag_use_flag                 = true;
    }
    /* else: no space available or trying to remove non-existent tag - silently fail */

    mlog_output_unlock();
}

/**
 * get the level on tag's level filer
 *
 * @param tag tag
 *
 * @return It will return the lowest level when tag was not found.
 *         Other level will return when tag was found.
 */
uint8_t mlog_get_filter_tag_lvl(const char* tag)
{
    if (tag == NULL)
    {
        return MLOG_FILTER_LVL_ALL;
    }

    if (!s_mlog.init_ok || tag[0] == '\0')
    {
        return MLOG_FILTER_LVL_ALL;
    }

    mlog_output_lock();

    uint8_t level = MLOG_FILTER_LVL_ALL;

    /* Linear search with early exit */
    for (uint8_t i = 0; i < MLOG_FILTER_TAG_LVL_MAX_NUM; i++)
    {
        if (s_mlog.filter.tag_lvl[i].tag_use_flag &&
            strncmp(tag, s_mlog.filter.tag_lvl[i].tag, MLOG_FILTER_TAG_MAX_LEN) == 0)
        {
            level = s_mlog.filter.tag_lvl[i].level;
            break;  // early exit
        }
    }

    mlog_output_unlock();
    return level;
}

/**
 * output RAW format log
 *
 * @param format output format
 * @param ... args
 */
void mlog_raw_output(const char* format, ...)
{
    va_list args;
    size_t  log_len = 0;
    int     fmt_result;

    /* check output enabled */
    if (!s_mlog.output_enabled)
    {
        return;
    }

    /* args point to the first variable parameter */
    va_start(args, format);

    /* 锁保护范围：格式化 + 输出共享 s_log_buf，必须在同一临界区 */
    /* lock output */
    mlog_output_lock();

    /* package log data to buffer */
    fmt_result = vsnprintf(s_log_buf, MLOG_LINE_BUF_SIZE, format, args);

    /* output converted log */
    if ((fmt_result > -1) && (fmt_result <= MLOG_LINE_BUF_SIZE))
    {
        log_len = fmt_result;
    }
    else
    {
        log_len = MLOG_LINE_BUF_SIZE;
    }
    /* output log */
    output_log_line(s_log_buf, log_len);
    mlog_output_unlock();
    va_end(args);
    ;
}

/**
 * 格式化日志头部：颜色起始 + 级别 + 标签（含对齐填充）
 * @return 写入 s_log_buf 的长度
 */
static size_t fmt_header_level_tag(uint8_t level, const char* tag, size_t tag_len)
{
    size_t log_len = 0;

#ifdef MLOG_COLOR_ENABLE
    if (s_mlog.text_color_enabled)
    {
        log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, CSI_START);
        log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, s_color_output_info[level]);
    }
#endif

    if (GET_FMT_ENABLED(level, MLOG_FMT_LVL))
    {
        log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, s_level_output_info[level]);
    }

    if (GET_FMT_ENABLED(level, MLOG_FMT_TAG))
    {
        log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, tag);
        if (tag_len <= MLOG_FILTER_TAG_MAX_LEN / 2)
        {
            char   tag_space[MLOG_FILTER_TAG_MAX_LEN / 2 + 1];
            size_t pad_len = (MLOG_FILTER_TAG_MAX_LEN / 2U) - tag_len;
            memset(tag_space, ' ', pad_len);
            tag_space[pad_len] = '\0';
            log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, tag_space);
        }
        log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, " ");
    }

    return log_len;
}

/**
 * 格式化时间/进程/线程信息段
 */
static size_t fmt_header_context(uint8_t level, size_t log_len)
{
    if (!GET_FMT_ENABLED(level, MLOG_FMT_TIME | MLOG_FMT_P_INFO | MLOG_FMT_T_INFO))
    {
        return log_len;
    }

    log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, "[");
    if (GET_FMT_ENABLED(level, MLOG_FMT_TIME))
    {
        if (s_port.get_time != NULL)
        {
            log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, s_port.get_time());
        }
        if (GET_FMT_ENABLED(level, MLOG_FMT_P_INFO | MLOG_FMT_T_INFO))
        {
            log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, " ");
        }
    }
    if (GET_FMT_ENABLED(level, MLOG_FMT_P_INFO))
    {
        if (s_port.get_p_info != NULL)
        {
            log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, s_port.get_p_info());
        }
        if (GET_FMT_ENABLED(level, MLOG_FMT_T_INFO))
        {
            log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, " ");
        }
    }
    if (GET_FMT_ENABLED(level, MLOG_FMT_T_INFO))
    {
        if (s_port.get_t_info != NULL)
        {
            log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, s_port.get_t_info());
        }
    }
    log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, "] ");

    return log_len;
}

/**
 * 格式化源码位置段：(文件:行号 函数名)
 */
static size_t fmt_header_source(uint8_t level, const char* file, const char* func, long line, size_t log_len)
{
    if (!GET_FMT_USED_PTR(level, MLOG_FMT_DIR, file) && !GET_FMT_USED_PTR(level, MLOG_FMT_FUNC, func) &&
        !GET_FMT_USED_INT(level, MLOG_FMT_LINE, line))
    {
        return log_len;
    }

    log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, "(");

    if (GET_FMT_USED_PTR(level, MLOG_FMT_DIR, file))
    {
        log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, file);
        if (GET_FMT_USED_PTR(level, MLOG_FMT_FUNC, func))
        {
            log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, ":");
        }
        else if (GET_FMT_USED_INT(level, MLOG_FMT_LINE, line))
        {
            log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, " ");
        }
    }

    if (GET_FMT_USED_INT(level, MLOG_FMT_LINE, line))
    {
        char line_num[MLOG_LINE_NUM_MAX_LEN + 1] = {0};
        snprintf(line_num, MLOG_LINE_NUM_MAX_LEN, "%ld", line);
        log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, line_num);
        if (GET_FMT_USED_PTR(level, MLOG_FMT_FUNC, func))
        {
            log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, " ");
        }
    }

    if (GET_FMT_USED_PTR(level, MLOG_FMT_FUNC, func))
    {
        log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, func);
    }

    log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, ")");

    return log_len;
}

/**
 * 追加颜色结束符和换行符，处理溢出截断
 */
static size_t fmt_tail(size_t log_len)
{
#ifdef MLOG_COLOR_ENABLE
    if (log_len + CSI_END_LEN + NEWLINE_LEN > MLOG_LINE_BUF_SIZE)
    {
        log_len = MLOG_LINE_BUF_SIZE - CSI_END_LEN - NEWLINE_LEN;
    }
    if (s_mlog.text_color_enabled)
    {
        log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, CSI_END);
    }
#else
    if (log_len + NEWLINE_LEN > MLOG_LINE_BUF_SIZE)
    {
        log_len = MLOG_LINE_BUF_SIZE - NEWLINE_LEN;
    }
#endif /* MLOG_COLOR_ENABLE */
    log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, MLOG_NEWLINE_SIGN);
    return log_len;
}

/**
 * output the log
 *
 * @param level level
 * @param tag tag
 * @param file file name
 * @param func function name
 * @param line line number
 * @param format output format
 * @param ... args
 */
void mlog_output(uint8_t level, const char* tag, const char* file, const char* func, const long line,
                 const char* format, ...)
{
    /* P1: 空指针防御 */
    if (tag == NULL)
    {
        tag = "NULL";
    }
    /* P6: level 越界防御（MLOG_ASSERT 可被关闭，此处硬编码保护） */
    if (level > MLOG_LVL_VERBOSE)
    {
        return;
    }

    MLOG_ASSERT(level <= MLOG_LVL_VERBOSE);

    /* check output enabled */
    if (!s_mlog.output_enabled)
    {
        return;
    }
    /* level filter */
    if (level > s_mlog.filter.level || level > mlog_get_filter_tag_lvl(tag))
    {
        return;
    }
    else if (s_mlog.filter.tag[0] != '\0' && strncmp(tag, s_mlog.filter.tag, MLOG_FILTER_TAG_MAX_LEN) != 0)
    { /* tag filter: exact match */
        return;
    }

    va_list args;
    va_start(args, format);

    /*
     * 锁保护范围说明：
     * s_log_buf 为静态共享缓冲区，格式化和输出必须在同一临界区内完成。
     * 如果锁实现为 __disable_irq()，此处会较长时间关中断。
     * 高频日志场景建议启用 MLOG_BUF_OUTPUT_ENABLE 缓冲模式，
     * 将实际 I/O 延迟到 mlog_flush() 中执行，以缩短临界区时间。
     */
    mlog_output_lock();

    /* 格式化日志头部 */
    size_t log_len = fmt_header_level_tag(level, tag, strlen(tag));
    log_len        = fmt_header_context(level, log_len);
    log_len        = fmt_header_source(level, file, func, line, log_len);

    /* 格式化用户消息 */
    int fmt_result = vsnprintf(s_log_buf + log_len, MLOG_LINE_BUF_SIZE - log_len, format, args);
    va_end(args);

    if ((fmt_result > -1) && (log_len + (size_t) fmt_result <= MLOG_LINE_BUF_SIZE))
    {
        log_len += (size_t) fmt_result;
    }
    else
    {
        log_len = MLOG_LINE_BUF_SIZE;
    }

    /* 追加颜色结束符和换行 */
    log_len = fmt_tail(log_len);

    /* 输出日志 */
    output_log_line(s_log_buf, log_len);

    mlog_output_unlock();
}

/**
 * Set a hook function to MLogger assert. It will run when the expression is false.
 *
 * @param hook the hook function
 */
void mlog_assert_set_hook(void (*hook)(const char* expr, const char* func, size_t line))
{
    g_mlog_assert_hook = hook;
}

/**
 * dump the hex format data to log
 *
 * @param name name for hex object, it will show on log header
 * @param width hex number for every line, such as: 16, 32
 * @param buf hex buffer
 * @param size buffer size
 */
void mlog_hexdump(const char* name, uint8_t width, const void* buf, uint16_t size)
{
#define IS_PRINTABLE(ch) ((unsigned int) ((ch) - ' ') < 127u - ' ')

    uint16_t       i, j;
    uint16_t       log_len        = 0;
    const uint8_t* buf_p          = buf;
    char           dump_string[8] = {0};
    int            fmt_result;

    /* P4/P5: 参数合法性防御 */
    if (name == NULL || buf == NULL || width == 0 || size == 0)
    {
        return;
    }

    if (!s_mlog.output_enabled)
    {
        return;
    }

    /* level filter */
    if (MLOG_LVL_DEBUG > s_mlog.filter.level)
    {
        return;
    }
    else if (s_mlog.filter.tag[0] != '\0' && strncmp(name, s_mlog.filter.tag, MLOG_FILTER_TAG_MAX_LEN) != 0)
    { /* tag filter: exact match */
        return;
    }

    /* lock output */
    mlog_output_lock();

    for (i = 0; i < size; i += width)
    {
        /* package header */
        fmt_result = snprintf(s_log_buf, MLOG_LINE_BUF_SIZE, "D/HEX %s: %04X-%04X: ", name, i, i + width - 1);
        /* calculate log length */
        if ((fmt_result > -1) && (fmt_result <= MLOG_LINE_BUF_SIZE))
        {
            log_len = fmt_result;
        }
        else
        {
            log_len = MLOG_LINE_BUF_SIZE;
        }
        /* dump hex */
        for (j = 0; j < width; j++)
        {
            if (i + j < size)
            {
                snprintf(dump_string, sizeof(dump_string), "%02X ", buf_p[i + j]);
            }
            else
            {
                strncpy(dump_string, "   ", sizeof(dump_string));
            }
            log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, dump_string);
            if ((j + 1) % 8 == 0)
            {
                log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, " ");
            }
        }
        log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, "  ");
        /* dump char for hex */
        for (j = 0; j < width; j++)
        {
            if (i + j < size)
            {
                snprintf(dump_string, sizeof(dump_string), "%c", IS_PRINTABLE(buf_p[i + j]) ? buf_p[i + j] : '.');
                log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, dump_string);
            }
        }
        /* overflow check and reserve some space for newline sign */
        if (log_len + NEWLINE_LEN > MLOG_LINE_BUF_SIZE)
        {
            log_len = MLOG_LINE_BUF_SIZE - NEWLINE_LEN;
        }
        /* package newline sign */
        log_len += MLOG_STRCPY(log_len, s_log_buf + log_len, MLOG_NEWLINE_SIGN);
        /* do log output */
#if defined(MLOG_BUF_OUTPUT_ENABLE)
        mlog_buf_output(s_log_buf, log_len);
#else
        if (s_port.output != NULL)
        {
            s_port.output(s_log_buf, log_len);
        }
#endif
    }
    /* unlock output */
    mlog_output_unlock();
}
