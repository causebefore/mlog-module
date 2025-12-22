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

#include <mlog.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

#if !defined(MLOG_FILTER_KW_MAX_LEN)
#error "Please configure output filter's keyword max length (in mlog_cfg.h)"
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

/* MLogger object */
static MLogger mlog;
/* every line log's buffer */
static char log_buf[MLOG_LINE_BUF_SIZE] = {0};
/* level output info */
static const char *level_output_info[] = {
    [MLOG_LVL_ASSERT]  = "A/",
    [MLOG_LVL_ERROR]   = "E/",
    [MLOG_LVL_WARN]    = "W/",
    [MLOG_LVL_INFO]    = "I/",
    [MLOG_LVL_DEBUG]   = "D/",
    [MLOG_LVL_VERBOSE] = "V/",
};

#ifdef MLOG_COLOR_ENABLE
/* color output info */
static const char *color_output_info[] = {
    [MLOG_LVL_ASSERT]  = MLOG_COLOR_ASSERT,
    [MLOG_LVL_ERROR]   = MLOG_COLOR_ERROR,
    [MLOG_LVL_WARN]    = MLOG_COLOR_WARN,
    [MLOG_LVL_INFO]    = MLOG_COLOR_INFO,
    [MLOG_LVL_DEBUG]   = MLOG_COLOR_DEBUG,
    [MLOG_LVL_VERBOSE] = MLOG_COLOR_VERBOSE,
};
#endif /* MLOG_COLOR_ENABLE */

static bool get_fmt_enabled(uint8_t level, size_t set);
static bool get_fmt_used_and_enabled_u32(uint8_t level, size_t set, uint32_t arg);
static bool get_fmt_used_and_enabled_ptr(uint8_t level, size_t set, const char *arg);
static void mlog_set_filter_tag_lvl_default(void);

/* MLogger assert hook */
void (*mlog_assert_hook)(const char *expr, const char *func, size_t line);

extern void mlog_port_output(const char *log, size_t size);
extern void mlog_port_output_lock(void);
extern void mlog_port_output_unlock(void);

MlogErrCode mlog_init(void)
{
    extern MlogErrCode mlog_port_init(void);
    extern MlogErrCode mlog_async_init(void);

    MlogErrCode result = MLOG_NO_ERR;

    if (mlog.init_ok == true)
    {
        return result;
    }

    /* port initialize */
    result = mlog_port_init();
    if (result != MLOG_NO_ERR)
    {
        return result;
    }

    /* enable the output lock */
    mlog_output_lock_enabled(true);
    /* output locked status initialize */
    mlog.output_is_locked_before_enable  = false;
    mlog.output_is_locked_before_disable = false;

#ifdef MLOG_COLOR_ENABLE
    /* enable text color by default */
    mlog_set_text_color_enabled(true);
#endif

    /* set level is MLOG_LVL_VERBOSE */
    mlog_set_filter_lvl(MLOG_LVL_VERBOSE);

    /* set tag_level to default val */
    mlog_set_filter_tag_lvl_default();

    mlog.init_ok = true;

    return result;
}

/**
 * MLogger deinitialize.
 *
 */
void mlog_deinit(void)
{
    extern MlogErrCode mlog_port_deinit(void);
    extern MlogErrCode mlog_async_deinit(void);

    if (!mlog.init_ok)
    {
        return;
    }

    /* port deinitialize */
    mlog_port_deinit();

    mlog.init_ok = false;
}

/**
 * MLogger start after initialize.
 */
void mlog_start(void)
{
    if (!mlog.init_ok)
    {
        return;
    }

    /* enable output */
    mlog_set_output_enabled(true);

#if defined(MLOG_BUF_OUTPUT_ENABLE)
    mlog_buf_enabled(true);
#endif

    /* show version */
    log_i("MLogger V%s is initialize success.", MLOG_SW_VERSION);
}

void mlog_stop(void)
{
    if (!mlog.init_ok)
    {
        return;
    }

    /* disable output */
    mlog_set_output_enabled(false);

#if defined(MLOG_BUF_OUTPUT_ENABLE)
    mlog_buf_enabled(false);
#endif

    /* show version */
    log_i("MLogger V%s is deinitialize success.", MLOG_SW_VERSION);
}

/**
 * set output enable or disable
 *
 * @param enabled TRUE: enable FALSE: disable
 */
void mlog_set_output_enabled(bool enabled)
{
    MLOG_ASSERT((enabled == false) || (enabled == true));

    mlog.output_enabled = enabled;
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

    mlog.text_color_enabled = enabled;
}

/**
 * get log text color enable status
 *
 * @return enable or disable
 */
bool mlog_get_text_color_enabled(void)
{
    return mlog.text_color_enabled;
}
#endif /* MLOG_COLOR_ENABLE */

/**
 * get output is enable or disable
 *
 * @return enable or disable
 */
bool mlog_get_output_enabled(void)
{
    return mlog.output_enabled;
}

/**
 * set log output format. only enable or disable
 *
 * @param level level
 * @param set format set
 */
void mlog_set_fmt(uint8_t level, size_t set)
{
    MLOG_ASSERT(level <= MLOG_LVL_VERBOSE);

    mlog.enabled_fmt_set[level] = set;
}

/**
 * set log filter all parameter
 *
 * @param level level
 * @param tag tag
 * @param keyword keyword
 */
void mlog_set_filter(uint8_t level, const char *tag, const char *keyword)
{
    MLOG_ASSERT(level <= MLOG_LVL_VERBOSE);

    mlog_set_filter_lvl(level);
    mlog_set_filter_tag(tag);
    mlog_set_filter_kw(keyword);
}

/**
 * set log filter's level
 *
 * @param level level
 */
void mlog_set_filter_lvl(uint8_t level)
{
    MLOG_ASSERT(level <= MLOG_LVL_VERBOSE);

    mlog.filter.level = level;
}

/**
 * set log filter's tag
 *
 * @param tag tag
 */
void mlog_set_filter_tag(const char *tag)
{
    strncpy(mlog.filter.tag, tag, MLOG_FILTER_TAG_MAX_LEN);
}

/**
 * set log filter's keyword
 *
 * @param keyword keyword
 */
void mlog_set_filter_kw(const char *keyword)
{
    strncpy(mlog.filter.keyword, keyword, MLOG_FILTER_KW_MAX_LEN);
}

/**
 * lock output
 */
void mlog_output_lock(void)
{
    if (mlog.output_lock_enabled)
    {
        mlog_port_output_lock();
        mlog.output_is_locked_before_disable = true;
    }
    else
    {
        mlog.output_is_locked_before_enable = true;
    }
}

/**
 * unlock output
 */
void mlog_output_unlock(void)
{
    if (mlog.output_lock_enabled)
    {
        mlog_port_output_unlock();
        mlog.output_is_locked_before_disable = false;
    }
    else
    {
        mlog.output_is_locked_before_enable = false;
    }
}

/**
 * set log filter's tag level val to default
 */
static void mlog_set_filter_tag_lvl_default(void)
{
    uint8_t i = 0;

    for (i = 0; i < MLOG_FILTER_TAG_LVL_MAX_NUM; i++)
    {
        memset(mlog.filter.tag_lvl[i].tag, '\0', MLOG_FILTER_TAG_MAX_LEN + 1);
        mlog.filter.tag_lvl[i].level        = MLOG_FILTER_LVL_SILENT;
        mlog.filter.tag_lvl[i].tag_use_flag = false;
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
void mlog_set_filter_tag_lvl(const char *tag, uint8_t level)
{
    MLOG_ASSERT(level <= MLOG_LVL_VERBOSE);
    MLOG_ASSERT(tag != ((void *) 0));
    uint8_t i = 0;

    if (!mlog.init_ok)
    {
        return;
    }

    mlog_output_lock();
    /* find the tag in arr */
    for (i = 0; i < MLOG_FILTER_TAG_LVL_MAX_NUM; i++)
    {
        if (mlog.filter.tag_lvl[i].tag_use_flag == true &&
            !strncmp(tag, mlog.filter.tag_lvl[i].tag, MLOG_FILTER_TAG_MAX_LEN))
        {
            break;
        }
    }

    if (i < MLOG_FILTER_TAG_LVL_MAX_NUM)
    {
        /* find OK */
        if (level == MLOG_FILTER_LVL_ALL)
        {
            /* remove current tag's level filter when input level is the lowest level */
            mlog.filter.tag_lvl[i].tag_use_flag = false;
            memset(mlog.filter.tag_lvl[i].tag, '\0', MLOG_FILTER_TAG_MAX_LEN + 1);
            mlog.filter.tag_lvl[i].level = MLOG_FILTER_LVL_SILENT;
        }
        else
        {
            mlog.filter.tag_lvl[i].level = level;
        }
    }
    else
    {
        /* only add the new tag's level filer when level is not MLOG_FILTER_LVL_ALL */
        if (level != MLOG_FILTER_LVL_ALL)
        {
            for (i = 0; i < MLOG_FILTER_TAG_LVL_MAX_NUM; i++)
            {
                if (mlog.filter.tag_lvl[i].tag_use_flag == false)
                {
                    strncpy(mlog.filter.tag_lvl[i].tag, tag, MLOG_FILTER_TAG_MAX_LEN);
                    mlog.filter.tag_lvl[i].level        = level;
                    mlog.filter.tag_lvl[i].tag_use_flag = true;
                    break;
                }
            }
        }
    }
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
uint8_t mlog_get_filter_tag_lvl(const char *tag)
{
    MLOG_ASSERT(tag != ((void *) 0));
    uint8_t i     = 0;
    uint8_t level = MLOG_FILTER_LVL_ALL;

    if (!mlog.init_ok)
    {
        return level;
    }

    mlog_output_lock();
    /* find the tag in arr */
    for (i = 0; i < MLOG_FILTER_TAG_LVL_MAX_NUM; i++)
    {
        if (mlog.filter.tag_lvl[i].tag_use_flag == true &&
            !strncmp(tag, mlog.filter.tag_lvl[i].tag, MLOG_FILTER_TAG_MAX_LEN))
        {
            level = mlog.filter.tag_lvl[i].level;
            break;
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
void mlog_raw_output(const char *format, ...)
{
    va_list args;
    size_t  log_len = 0;
    int     fmt_result;

    /* check output enabled */
    if (!mlog.output_enabled)
    {
        return;
    }

    /* args point to the first variable parameter */
    va_start(args, format);

    /* lock output */
    mlog_output_lock();

    /* package log data to buffer */
    fmt_result = vsnprintf(log_buf, MLOG_LINE_BUF_SIZE, format, args);

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
#if defined(MLOG_BUF_OUTPUT_ENABLE)
    extern void mlog_buf_output(const char *log, size_t size);
    mlog_buf_output(log_buf, log_len);
#else
    mlog_port_output(log_buf, log_len);
#endif
    /* unlock output */
    mlog_output_unlock();
    mlog_memset(log_buf, 0, sizeof(log_buf));
    va_end(args);
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
 *
 */
void mlog_output(uint8_t     level,
                 const char *tag,
                 const char *file,
                 const char *func,
                 const long  line,
                 const char *format,
                 ...)
{
    extern const char *mlog_port_get_time(void);
    extern const char *mlog_port_get_p_info(void);
    extern const char *mlog_port_get_t_info(void);

    size_t  tag_len = strlen(tag), log_len = 0, newline_len = strlen(MLOG_NEWLINE_SIGN);
    char    line_num[MLOG_LINE_NUM_MAX_LEN + 1]        = {0};
    char    tag_sapce[MLOG_FILTER_TAG_MAX_LEN / 2 + 1] = {0};
    va_list args;
    int     fmt_result;

    MLOG_ASSERT(level <= MLOG_LVL_VERBOSE);

    /* check output enabled */
    if (!mlog.output_enabled)
    {
        return;
    }
    /* level filter */
    if (level > mlog.filter.level || level > mlog_get_filter_tag_lvl(tag))
    {
        return;
    }
    else if (!strstr(tag, mlog.filter.tag))
    { /* tag filter */
        return;
    }
    /* args point to the first variable parameter */
    va_start(args, format);
    /* lock output */
    mlog_output_lock();

#ifdef MLOG_COLOR_ENABLE
    /* add CSI start sign and color info */
    if (mlog.text_color_enabled)
    {
        log_len += mlog_strcpy(log_len, log_buf + log_len, CSI_START);
        log_len += mlog_strcpy(log_len, log_buf + log_len, color_output_info[level]);
    }
#endif

    /* package level info */
    if (get_fmt_enabled(level, MLOG_FMT_LVL))
    {
        log_len += mlog_strcpy(log_len, log_buf + log_len, level_output_info[level]);
    }
    /* package tag info */
    if (get_fmt_enabled(level, MLOG_FMT_TAG))
    {
        log_len += mlog_strcpy(log_len, log_buf + log_len, tag);
        /* if the tag length is less than 50% MLOG_FILTER_TAG_MAX_LEN, then fill space */
        if (tag_len <= MLOG_FILTER_TAG_MAX_LEN / 2)
        {
            memset(tag_sapce, ' ', MLOG_FILTER_TAG_MAX_LEN / 2 - tag_len);
            log_len += mlog_strcpy(log_len, log_buf + log_len, tag_sapce);
        }
        log_len += mlog_strcpy(log_len, log_buf + log_len, " ");
    }
    /* package time, process and thread info */
    if (get_fmt_enabled(level, MLOG_FMT_TIME | MLOG_FMT_P_INFO | MLOG_FMT_T_INFO))
    {
        log_len += mlog_strcpy(log_len, log_buf + log_len, "[");
        /* package time info */
        if (get_fmt_enabled(level, MLOG_FMT_TIME))
        {
            log_len += mlog_strcpy(log_len, log_buf + log_len, mlog_port_get_time());
            if (get_fmt_enabled(level, MLOG_FMT_P_INFO | MLOG_FMT_T_INFO))
            {
                log_len += mlog_strcpy(log_len, log_buf + log_len, " ");
            }
        }
        /* package process info */
        if (get_fmt_enabled(level, MLOG_FMT_P_INFO))
        {
            log_len += mlog_strcpy(log_len, log_buf + log_len, mlog_port_get_p_info());
            if (get_fmt_enabled(level, MLOG_FMT_T_INFO))
            {
                log_len += mlog_strcpy(log_len, log_buf + log_len, " ");
            }
        }
        /* package thread info */
        if (get_fmt_enabled(level, MLOG_FMT_T_INFO))
        {
            log_len += mlog_strcpy(log_len, log_buf + log_len, mlog_port_get_t_info());
        }
        log_len += mlog_strcpy(log_len, log_buf + log_len, "] ");
    }
    /* package file directory and name, function name and line number info */
    if (get_fmt_used_and_enabled_ptr(level, MLOG_FMT_DIR, file) ||
        get_fmt_used_and_enabled_ptr(level, MLOG_FMT_FUNC, func) ||
        get_fmt_used_and_enabled_u32(level, MLOG_FMT_LINE, line))
    {
        log_len += mlog_strcpy(log_len, log_buf + log_len, "(");
        /* package file info */
        if (get_fmt_used_and_enabled_ptr(level, MLOG_FMT_DIR, file))
        {
            log_len += mlog_strcpy(log_len, log_buf + log_len, file);
            if (get_fmt_used_and_enabled_ptr(level, MLOG_FMT_FUNC, func))
            {
                log_len += mlog_strcpy(log_len, log_buf + log_len, ":");
            }
            else if (get_fmt_used_and_enabled_u32(level, MLOG_FMT_LINE, line))
            {
                log_len += mlog_strcpy(log_len, log_buf + log_len, " ");
            }
        }
        /* package line info */
        if (get_fmt_used_and_enabled_u32(level, MLOG_FMT_LINE, line))
        {
            snprintf(line_num, MLOG_LINE_NUM_MAX_LEN, "%ld", line);
            log_len += mlog_strcpy(log_len, log_buf + log_len, line_num);
            if (get_fmt_used_and_enabled_ptr(level, MLOG_FMT_FUNC, func))
            {
                log_len += mlog_strcpy(log_len, log_buf + log_len, " ");
            }
        }
        /* package func info */
        if (get_fmt_used_and_enabled_ptr(level, MLOG_FMT_FUNC, func))
        {
            log_len += mlog_strcpy(log_len, log_buf + log_len, func);
        }
        log_len += mlog_strcpy(log_len, log_buf + log_len, ")");
    }
    /* package other log data to buffer. '\0' must be added in the end by vsnprintf. */
    fmt_result = vsnprintf(log_buf + log_len, MLOG_LINE_BUF_SIZE - log_len, format, args);

    va_end(args);
    /* calculate log length */
    if ((log_len + fmt_result <= MLOG_LINE_BUF_SIZE) && (fmt_result > -1))
    {
        log_len += fmt_result;
    }
    else
    {
        /* using max length */
        log_len = MLOG_LINE_BUF_SIZE;
    }
    /* overflow check and reserve some space for CSI end sign and newline sign */
#ifdef MLOG_COLOR_ENABLE
    if (log_len + (sizeof(CSI_END) - 1) + newline_len > MLOG_LINE_BUF_SIZE)
    {
        /* using max length */
        log_len = MLOG_LINE_BUF_SIZE;
        /* reserve some space for CSI end sign */
        log_len -= (sizeof(CSI_END) - 1);
#else
    if (log_len + newline_len > MLOG_LINE_BUF_SIZE)
    {
        /* using max length */
        log_len = MLOG_LINE_BUF_SIZE;
#endif /* MLOG_COLOR_ENABLE */
        /* reserve some space for newline sign */
        log_len -= newline_len;
    }
    /* keyword filter */
    if (mlog.filter.keyword[0] != '\0')
    {
        /* add string end sign */
        log_buf[log_len] = '\0';
        /* find the keyword */
        if (!strstr(log_buf, mlog.filter.keyword))
        {
            /* unlock output */
            mlog_output_unlock();
            return;
        }
    }

#ifdef MLOG_COLOR_ENABLE
    /* add CSI end sign */
    if (mlog.text_color_enabled)
    {
        log_len += mlog_strcpy(log_len, log_buf + log_len, CSI_END);
    }
#endif

    /* package newline sign */
    log_len += mlog_strcpy(log_len, log_buf + log_len, MLOG_NEWLINE_SIGN);
    /* output log */
#if defined(MLOG_BUF_OUTPUT_ENABLE)
    extern void mlog_buf_output(const char *log, size_t size);
    mlog_buf_output(log_buf, log_len);
#else
    mlog_port_output(log_buf, log_len);
#endif
    /* unlock output */
    mlog_output_unlock();

    memset(log_buf, 0, sizeof(log_buf));
}

/**
 * get format enabled
 *
 * @param level level
 * @param set format set
 *
 * @return enable or disable
 */
static bool get_fmt_enabled(uint8_t level, size_t set)
{
    MLOG_ASSERT(level <= MLOG_LVL_VERBOSE);

    if (mlog.enabled_fmt_set[level] & set)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static bool get_fmt_used_and_enabled_u32(uint8_t level, size_t set, uint32_t arg)
{
    return arg && get_fmt_enabled(level, set);
}
static bool get_fmt_used_and_enabled_ptr(uint8_t level, size_t set, const char *arg)
{
    return arg && get_fmt_enabled(level, set);
}

/**
 * enable or disable logger output lock
 * @note disable this lock is not recommended except you want output system exception log
 *
 * @param enabled true: enable  false: disable
 */
void mlog_output_lock_enabled(bool enabled)
{
    mlog.output_lock_enabled = enabled;
    /* it will re-lock or re-unlock before output lock enable */
    if (mlog.output_lock_enabled)
    {
        if (!mlog.output_is_locked_before_disable && mlog.output_is_locked_before_enable)
        {
            /* the output lock is unlocked before disable, and the lock will unlocking after enable */
            mlog_port_output_lock();
        }
        else if (mlog.output_is_locked_before_disable && !mlog.output_is_locked_before_enable)
        {
            /* the output lock is locked before disable, and the lock will locking after enable */
            mlog_port_output_unlock();
        }
    }
}

/**
 * Set a hook function to MLogger assert. It will run when the expression is false.
 *
 * @param hook the hook function
 */
void mlog_assert_set_hook(void (*hook)(const char *expr, const char *func, size_t line))
{
    mlog_assert_hook = hook;
}

/**
 * find the log level
 * @note make sure the log level is output on each format
 *
 * @param log log buffer
 *
 * @return log level, found failed will return -1
 */
int8_t mlog_find_lvl(const char *log)
{
    MLOG_ASSERT(log);
    /* make sure the log level is output on each format */
    MLOG_ASSERT(mlog.enabled_fmt_set[MLOG_LVL_ASSERT] & MLOG_FMT_LVL);
    MLOG_ASSERT(mlog.enabled_fmt_set[MLOG_LVL_ERROR] & MLOG_FMT_LVL);
    MLOG_ASSERT(mlog.enabled_fmt_set[MLOG_LVL_WARN] & MLOG_FMT_LVL);
    MLOG_ASSERT(mlog.enabled_fmt_set[MLOG_LVL_INFO] & MLOG_FMT_LVL);
    MLOG_ASSERT(mlog.enabled_fmt_set[MLOG_LVL_DEBUG] & MLOG_FMT_LVL);
    MLOG_ASSERT(mlog.enabled_fmt_set[MLOG_LVL_VERBOSE] & MLOG_FMT_LVL);

#ifdef MLOG_COLOR_ENABLE
    uint8_t i;
    size_t  csi_start_len = strlen(CSI_START);
    for (i = 0; i < MLOG_LVL_TOTAL_NUM; i++)
    {
        if (!strncmp(color_output_info[i], log + csi_start_len, strlen(color_output_info[i])))
        {
            return i;
        }
    }
    /* found failed */
    return -1;
#else
    switch (log[0])
    {
        case 'A':
            return MLOG_LVL_ASSERT;
        case 'E':
            return MLOG_LVL_ERROR;
        case 'W':
            return MLOG_LVL_WARN;
        case 'I':
            return MLOG_LVL_INFO;
        case 'D':
            return MLOG_LVL_DEBUG;
        case 'V':
            return MLOG_LVL_VERBOSE;
        default:
            return -1;
    }
#endif
}

/**
 * find the log tag
 * @note make sure the log tag is output on each format
 * @note the tag don't have space in it
 *
 * @param log log buffer
 * @param lvl log level, you can get it by @see mlog_find_lvl
 * @param tag_len found tag length
 *
 * @return log tag, found failed will return NULL
 */
const char *mlog_find_tag(const char *log, uint8_t lvl, size_t *tag_len)
{
    const char *tag = NULL, *tag_end = NULL;

    MLOG_ASSERT(log);
    MLOG_ASSERT(tag_len);
    MLOG_ASSERT(lvl < MLOG_LVL_TOTAL_NUM);
    /* make sure the log tag is output on each format */
    MLOG_ASSERT(mlog.enabled_fmt_set[lvl] & MLOG_FMT_TAG);

#ifdef MLOG_COLOR_ENABLE
    tag = log + strlen(CSI_START) + strlen(color_output_info[lvl]) + strlen(level_output_info[lvl]);
#else
    tag = log + strlen(level_output_info[lvl]);
#endif
    /* find the first space after tag */
    if ((tag_end = memchr(tag, ' ', MLOG_FILTER_TAG_MAX_LEN)) != NULL)
    {
        *tag_len = tag_end - tag;
    }
    else
    {
        tag = NULL;
    }

    return tag;
}

/**
 * dump the hex format data to log
 *
 * @param name name for hex object, it will show on log header
 * @param width hex number for every line, such as: 16, 32
 * @param buf hex buffer
 * @param size buffer size
 */
void mlog_hexdump(const char *name, uint8_t width, const void *buf, uint16_t size)
{
#define __is_print(ch) ((unsigned int) ((ch) - ' ') < 127u - ' ')

    uint16_t       i, j;
    uint16_t       log_len        = 0;
    const uint8_t *buf_p          = buf;
    char           dump_string[8] = {0};
    int            fmt_result;

    if (!mlog.output_enabled)
    {
        return;
    }

    /* level filter */
    if (MLOG_LVL_DEBUG > mlog.filter.level)
    {
        return;
    }
    else if (!strstr(name, mlog.filter.tag))
    { /* tag filter */
        return;
    }

    /* lock output */
    mlog_output_lock();

    for (i = 0; i < size; i += width)
    {
        /* package header */
        fmt_result = snprintf(log_buf, MLOG_LINE_BUF_SIZE, "D/HEX %s: %04X-%04X: ", name, i, i + width - 1);
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
            log_len += mlog_strcpy(log_len, log_buf + log_len, dump_string);
            if ((j + 1) % 8 == 0)
            {
                log_len += mlog_strcpy(log_len, log_buf + log_len, " ");
            }
        }
        log_len += mlog_strcpy(log_len, log_buf + log_len, "  ");
        /* dump char for hex */
        for (j = 0; j < width; j++)
        {
            if (i + j < size)
            {
                snprintf(dump_string, sizeof(dump_string), "%c", __is_print(buf_p[i + j]) ? buf_p[i + j] : '.');
                log_len += mlog_strcpy(log_len, log_buf + log_len, dump_string);
            }
        }
        /* overflow check and reserve some space for newline sign */
        if (log_len + strlen(MLOG_NEWLINE_SIGN) > MLOG_LINE_BUF_SIZE)
        {
            log_len = MLOG_LINE_BUF_SIZE - strlen(MLOG_NEWLINE_SIGN);
        }
        /* package newline sign */
        log_len += mlog_strcpy(log_len, log_buf + log_len, MLOG_NEWLINE_SIGN);
        /* do log output */
#if defined(MLOG_BUF_OUTPUT_ENABLE)
        extern void mlog_buf_output(const char *log, size_t size);
        mlog_buf_output(log_buf, log_len);
#else
        mlog_port_output(log_buf, log_len);
#endif
    }
    /* unlock output */
    mlog_output_unlock();
    memset(log_buf, 0, sizeof(log_buf));
}
