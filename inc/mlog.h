/***
 * @Author: liu
 * @Date: 2025-12-09 14:59:19
 * @LastEditors: liu lbq08@foxmail.com
 * @LastEditTime: 2025-12-09 15:26:39
 * @FilePath: \Mlog\inc\mlog.h
 * @Description: mlog的内核头文件，包含日志级别定义、宏定义和API声明，通常不需要修改
 * @
 * @Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 */

#ifndef __INC_MLOG_H
#define __INC_MLOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <mlog_cfg.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* MLogger error code */
    typedef enum
    {
        MLOG_ERR_PORT_INIT_FAIL = -1,
        MLOG_NO_ERR,
    } MlogErrCode;

    /* Port callback function types - use _fn_t suffix to avoid name collision with wrapper functions */
    typedef MlogErrCode (*mlog_port_init_fn_t)(void);
    typedef void (*mlog_port_deinit_fn_t)(void);
    typedef void (*mlog_port_output_fn_t)(const char* log, size_t size);
    typedef void (*mlog_port_output_lock_fn_t)(void);
    typedef void (*mlog_port_output_unlock_fn_t)(void);
    typedef const char* (*mlog_port_get_time_fn_t)(void);
    typedef const char* (*mlog_port_get_p_info_fn_t)(void);
    typedef const char* (*mlog_port_get_t_info_fn_t)(void);

    /* Port interface structure */
    typedef struct
    {
        mlog_port_init_fn_t          init;
        mlog_port_deinit_fn_t        deinit;
        mlog_port_output_fn_t        output;
        mlog_port_output_lock_fn_t   output_lock;
        mlog_port_output_unlock_fn_t output_unlock;
        mlog_port_get_time_fn_t      get_time;
        mlog_port_get_p_info_fn_t    get_p_info;
        mlog_port_get_t_info_fn_t    get_t_info;
    } MlogPortInterface;

    /* output log's level */
    typedef enum
    {
        MLOG_LVL_ASSERT = 0,
        MLOG_LVL_ERROR,
        MLOG_LVL_WARN,
        MLOG_LVL_INFO,
        MLOG_LVL_DEBUG,
        MLOG_LVL_VERBOSE,
    } MlogLevel;

/* the output silent level and all level for filter setting */
#define MLOG_FILTER_LVL_SILENT MLOG_LVL_ASSERT
#define MLOG_FILTER_LVL_ALL    MLOG_LVL_VERBOSE

/* output log's level total number */
#define MLOG_LVL_TOTAL_NUM 6

/* MLogger assert for developer. */
#ifdef MLOG_ASSERT_ENABLE
    #define MLOG_ASSERT(EXPR)                                                                                          \
        do                                                                                                             \
        {                                                                                                              \
            if (!(EXPR))                                                                                               \
            {                                                                                                          \
                if (mlog_assert_hook != NULL)                                                                          \
                {                                                                                                      \
                    mlog_assert_hook(#EXPR, __func__, __LINE__);                                                       \
                }                                                                                                      \
                else                                                                                                   \
                {                                                                                                      \
                    mlog_a("mlog", "(%s) has assert failed at %s:%ld.", #EXPR, __func__, __LINE__);                    \
                }                                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
        while (0)
#else
    #define MLOG_ASSERT(EXPR) ((void) 0)
#endif

#ifndef MLOG_OUTPUT_ENABLE
    #define mlog_raw(...)
    #define mlog_assert(tag, ...)
    #define mlog_error(tag, ...)
    #define mlog_warn(tag, ...)
    #define mlog_info(tag, ...)
    #define mlog_debug(tag, ...)
    #define mlog_verbose(tag, ...)
#else /* MLOG_OUTPUT_ENABLE */

    #ifdef MLOG_FMT_USING_FUNC
        #define MLOG_OUTPUT_FUNC __func__
    #else
        #define MLOG_OUTPUT_FUNC NULL
    #endif

    #ifdef MLOG_FMT_USING_DIR
        #define MLOG_OUTPUT_DIR __FILE__
    #else
        #define MLOG_OUTPUT_DIR NULL
    #endif

    #ifdef MLOG_FMT_USING_LINE
        #define MLOG_OUTPUT_LINE __LINE__
    #else
        #define MLOG_OUTPUT_LINE 0
    #endif

    #define mlog_raw(...) mlog_raw_output(__VA_ARGS__)
    #if MLOG_OUTPUT_LVL >= MLOG_LVL_ASSERT
        #define mlog_assert(tag, ...)                                                                                  \
            mlog_output(MLOG_LVL_ASSERT, tag, MLOG_OUTPUT_DIR, MLOG_OUTPUT_FUNC, MLOG_OUTPUT_LINE, __VA_ARGS__)
    #else
        #define mlog_assert(tag, ...)
    #endif /* MLOG_OUTPUT_LVL >= MLOG_LVL_ASSERT */

    #if MLOG_OUTPUT_LVL >= MLOG_LVL_ERROR
        #define mlog_error(tag, ...)                                                                                   \
            mlog_output(MLOG_LVL_ERROR, tag, MLOG_OUTPUT_DIR, MLOG_OUTPUT_FUNC, MLOG_OUTPUT_LINE, __VA_ARGS__)
    #else
        #define mlog_error(tag, ...)
    #endif /* MLOG_OUTPUT_LVL >= MLOG_LVL_ERROR */

    #if MLOG_OUTPUT_LVL >= MLOG_LVL_WARN
        #define mlog_warn(tag, ...)                                                                                    \
            mlog_output(MLOG_LVL_WARN, tag, MLOG_OUTPUT_DIR, MLOG_OUTPUT_FUNC, MLOG_OUTPUT_LINE, __VA_ARGS__)
    #else
        #define mlog_warn(tag, ...)
    #endif /* MLOG_OUTPUT_LVL >= MLOG_LVL_WARN */

    #if MLOG_OUTPUT_LVL >= MLOG_LVL_INFO
        #define mlog_info(tag, ...)                                                                                    \
            mlog_output(MLOG_LVL_INFO, tag, MLOG_OUTPUT_DIR, MLOG_OUTPUT_FUNC, MLOG_OUTPUT_LINE, __VA_ARGS__)
    #else
        #define mlog_info(tag, ...)
    #endif /* MLOG_OUTPUT_LVL >= MLOG_LVL_INFO */

    #if MLOG_OUTPUT_LVL >= MLOG_LVL_DEBUG
        #define mlog_debug(tag, ...)                                                                                   \
            mlog_output(MLOG_LVL_DEBUG, tag, MLOG_OUTPUT_DIR, MLOG_OUTPUT_FUNC, MLOG_OUTPUT_LINE, __VA_ARGS__)
    #else
        #define mlog_debug(tag, ...)
    #endif /* MLOG_OUTPUT_LVL >= MLOG_LVL_DEBUG */

    #if MLOG_OUTPUT_LVL == MLOG_LVL_VERBOSE
        #define mlog_verbose(tag, ...)                                                                                 \
            mlog_output(MLOG_LVL_VERBOSE, tag, MLOG_OUTPUT_DIR, MLOG_OUTPUT_FUNC, MLOG_OUTPUT_LINE, __VA_ARGS__)
    #else
        #define mlog_verbose(tag, ...)
    #endif /* MLOG_OUTPUT_LVL == MLOG_LVL_VERBOSE */
#endif     /* MLOG_OUTPUT_ENABLE */

    /* all formats index */
    typedef enum
    {
        MLOG_FMT_LVL    = 1 << 0, /**< level */
        MLOG_FMT_TAG    = 1 << 1, /**< tag */
        MLOG_FMT_TIME   = 1 << 2, /**< current time */
        MLOG_FMT_P_INFO = 1 << 3, /**< process info */
        MLOG_FMT_T_INFO = 1 << 4, /**< thread info */
        MLOG_FMT_DIR    = 1 << 5, /**< file directory and name */
        MLOG_FMT_FUNC   = 1 << 6, /**< function name */
        MLOG_FMT_LINE   = 1 << 7, /**< line number */
    } MlogFmtIndex;

/* macro definition for all formats */
#define MLOG_FMT_ALL                                                                                                   \
    (MLOG_FMT_LVL | MLOG_FMT_TAG | MLOG_FMT_TIME | MLOG_FMT_P_INFO | MLOG_FMT_T_INFO | MLOG_FMT_DIR | MLOG_FMT_FUNC |  \
     MLOG_FMT_LINE)

    /* output log's tag filter */
    typedef struct
    {
        uint8_t level;
        char    tag[MLOG_FILTER_TAG_MAX_LEN + 1];
        bool    tag_use_flag; /**< false : tag is no used   true: tag is used */
    } MlogTagLvlFilter, *MlogTagLvlFilter_t;

    /* output log's filter */
    typedef struct
    {
        uint8_t          level;
        char             tag[MLOG_FILTER_TAG_MAX_LEN + 1];
        MlogTagLvlFilter tag_lvl[MLOG_FILTER_TAG_LVL_MAX_NUM];
    } MlogFilter, *MlogFilter_t;

    /* easy logger */
    typedef struct
    {
        MlogFilter filter;
        size_t     enabled_fmt_set[MLOG_LVL_TOTAL_NUM];
        bool       init_ok;
        bool       output_enabled;
#ifdef MLOG_COLOR_ENABLE
        bool text_color_enabled;
#endif

    } MLogger, *MLogger_t;

    /* mlog.c */
    MlogErrCode mlog_init(void);
    void        mlog_deinit(void);
    void        mlog_start(void);
    void        mlog_stop(void);


    void    mlog_set_text_color_enabled(bool enabled);
    bool    mlog_get_text_color_enabled(void);
    void    mlog_set_fmt(uint8_t level, size_t set);
    void    mlog_set_filter(uint8_t level, const char* tag);
    void    mlog_set_filter_lvl(uint8_t level);
    uint8_t mlog_get_filter_lvl(void);
    void    mlog_set_filter_tag(const char* tag);
    void    mlog_set_filter_tag_lvl(const char* tag, uint8_t level);
    uint8_t mlog_get_filter_tag_lvl(const char* tag);
    const char* mlog_get_level_name(uint8_t level);
    bool    mlog_is_level_enabled(uint8_t level);
    void    mlog_raw_output(const char* format, ...);
    void    mlog_output(uint8_t level, const char* tag, const char* file, const char* func, const long line,
                        const char* format, ...);
    void    mlog_output_lock(void);
    void    mlog_output_unlock(void);

    /* Port interface registration - call before mlog_init() */
    void mlog_port_register(const MlogPortInterface* iface);

    /* Get default port interface - call mlog_port_register(mlog_port_get_default()) before mlog_init() */
    const MlogPortInterface* mlog_port_get_default(void);

    extern void (*mlog_assert_hook)(const char* expr, const char* func, size_t line);
    void mlog_assert_set_hook(void (*hook)(const char* expr, const char* func, size_t line));
    void mlog_hexdump(const char* name, uint8_t width, const void* buf, uint16_t size);

#define mlog_a(tag, ...) mlog_assert(tag, __VA_ARGS__)
#define mlog_e(tag, ...) mlog_error(tag, __VA_ARGS__)
#define mlog_w(tag, ...) mlog_warn(tag, __VA_ARGS__)
#define mlog_i(tag, ...) mlog_info(tag, __VA_ARGS__)
#define mlog_d(tag, ...) mlog_debug(tag, __VA_ARGS__)
#define mlog_v(tag, ...) mlog_verbose(tag, __VA_ARGS__)

/**
 * log API short definition
 * NOTE: The `LOG_TAG` and `LOG_LVL` must defined before including the <mlog.h> when you want to use log_x API.
 */
#if !defined(LOG_TAG)
    #define LOG_TAG "NO_TAG"
#endif
#if !defined(LOG_LVL)
    #define LOG_LVL MLOG_LVL_VERBOSE
#endif
#if LOG_LVL >= MLOG_LVL_ASSERT
    #define log_a(...) mlog_a(LOG_TAG, __VA_ARGS__)
    #define loga(...)  mlog_a(LOG_TAG, __VA_ARGS__)
#else
    #define log_a(...) ((void) 0);
#endif
#if LOG_LVL >= MLOG_LVL_ERROR
    #define log_e(...) mlog_e(LOG_TAG, __VA_ARGS__)
    #define loge(...)  mlog_e(LOG_TAG, __VA_ARGS__)
#else
    #define log_e(...) ((void) 0);
#endif
#if LOG_LVL >= MLOG_LVL_WARN
    #define log_w(...) mlog_w(LOG_TAG, __VA_ARGS__)
    #define logw(...)  mlog_w(LOG_TAG, __VA_ARGS__)
#else
    #define log_w(...) ((void) 0);
#endif
#if LOG_LVL >= MLOG_LVL_INFO
    #define log_i(...) mlog_i(LOG_TAG, __VA_ARGS__)
    #define logi(...)  mlog_i(LOG_TAG, __VA_ARGS__)
#else
    #define log_i(...) ((void) 0);
#endif
#if LOG_LVL >= MLOG_LVL_DEBUG
    #define log_d(...) mlog_d(LOG_TAG, __VA_ARGS__)
    #define logd(...)  mlog_d(LOG_TAG, __VA_ARGS__)
#else
    #define log_d(...) ((void) 0);
#endif
#if LOG_LVL >= MLOG_LVL_VERBOSE
    #define log_v(...) mlog_v(LOG_TAG, __VA_ARGS__)
    #define logv(...)  mlog_v(LOG_TAG, __VA_ARGS__)
#else
    #define log_v(...) ((void) 0);
#endif

/* assert API short definition */
#if !defined(assert)
    #define assert MLOG_ASSERT
#endif

    /* mlog_buf.c */
    void mlog_buf_enabled(bool enabled);
    void mlog_flush(void);
#ifdef __cplusplus
}
#endif

#endif /* __INC_MLOG_H */
