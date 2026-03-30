/***
 * @Author: liu
 * @Date: 2025-12-09 14:59:19
 * @LastEditors: liu lbq08@foxmail.com
 * @LastEditTime: 2025-12-09 15:27:52
 * @FilePath: \Mlog\inc\mlog_cfg.h
 * @Description:
 * @
 * @Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 */

#ifndef MLOG_CFG_H_
#define MLOG_CFG_H_


/*---------------------------------------------------------------------------*/

#define MLOG_OUTPUT_ENABLE /* 使能输出日志 */
#define MLOG_ASSERT_ENABLE /* 使能断言检查 */


#define MLOG_OUTPUT_LVL       MLOG_LVL_VERBOSE /* 设置静态输出日志级别。范围：从 MLOG_LVL_ASSERT 到 MLOG_LVL_VERBOSE */
#define MLOG_LINE_BUF_SIZE    256              /* 每行日志的缓冲区大小 */
#define MLOG_LINE_NUM_MAX_LEN 5                /* 每行日志的行号最大长度 */
#define MLOG_FILTER_TAG_MAX_LEN     15         /* 输出过滤器的标签最大长度 */
#define MLOG_FILTER_KW_MAX_LEN      16         /* 输出过滤器的关键字最大长度 */
#define MLOG_FILTER_TAG_LVL_MAX_NUM 5          /* 输出过滤器的标签级别最大数量 */
#define MLOG_NEWLINE_SIGN           "\n"       /* 输出换行符 */

/*---------------------------------------------------------------------------*/
// #define USE_RTT_COLOR
/* 使能日志颜色 */
#define MLOG_COLOR_ENABLE
/* 日志颜色配置 */
#if defined(USE_RTT_COLOR)
    #define MLOG_COLOR_ASSERT  RTT_CTRL_TEXT_BRIGHT_MAGENTA /* 断言: 亮紫色 */
    #define MLOG_COLOR_ERROR   RTT_CTRL_TEXT_BRIGHT_RED     /* 错误: 亮红色 */
    #define MLOG_COLOR_WARN    RTT_CTRL_TEXT_BRIGHT_YELLOW  /* 警告: 亮黄色 */
    #define MLOG_COLOR_INFO    RTT_CTRL_TEXT_BRIGHT_GREEN   /* 信息: 亮绿色 */
    #define MLOG_COLOR_DEBUG   RTT_CTRL_TEXT_BRIGHT_CYAN    /* 调试: 亮青色 */
    #define MLOG_COLOR_VERBOSE RTT_CTRL_TEXT_BRIGHT_WHITE   /* 详细: 亮白色 */
#else
    /* ANSI 颜色格式: "前景色;背景色(可选);样式m"  参考: https://en.wikipedia.org/wiki/ANSI_escape_code */
    #define MLOG_COLOR_ASSERT  "35;22m" /* 紫色 普通 */
    #define MLOG_COLOR_ERROR   "31;22m" /* 红色 普通 */
    #define MLOG_COLOR_WARN    "33;22m" /* 黄色 普通 */
    #define MLOG_COLOR_INFO    "36;22m" /* 青色 普通 */
    #define MLOG_COLOR_DEBUG   "32;22m" /* 绿色 普通 */
    #define MLOG_COLOR_VERBOSE "34;22m" /* 蓝色 普通 */
#endif

/*---------------------------------------------------------------------------*/
/* 使能日志格式 */
/* 注释掉这些宏，如果你不想输出它们 */
#define MLOG_FMT_USING_FUNC
#define MLOG_FMT_USING_DIR
#define MLOG_FMT_USING_LINE

/*---------------------------------------------------------------------------*/
/* 内部使用的字符串和内存操作函数 */
#define MLOG_STRCPY mlog_strcpy
#define MLOG_MEMCPY memcpy
#define MLOG_MEMSET memset


#endif /* MLOG_CFG_H_ */
