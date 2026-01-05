/*
 * @Author: liu
 * @Date: 2025-12-09 14:59:19
 * @LastEditors: liu lbq08@foxmail.com
 * @LastEditTime: 2025-12-18 15:29:46
 * @FilePath: \Projectc:\Users\lbqdl\Desktop\demo_proj\app\Middleware\Mlog\port\mlog_port.c
 * @Description: mlog的移植层代码文件，用户需根据具体平台实现相关接口
 *
 * Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 */

#include "SEGGER_RTT.h"
#include "bsp_timer.h"
#include "bsp_usart.h"
#include "common.h"
#include "stm32f10x.h"

#include <mlog.h>
/**
 * MLogger port initialize
 *
 * @return result
 */
MlogErrCode mlog_port_init(void)
{
    MlogErrCode result = MLOG_NO_ERR;

    /* add your code here */

    return result;
}

/**
 * MLogger port deinitialize
 *
 */
void mlog_port_deinit(void)
{
    /* add your code here */
}

/**
 * output log port interface
 *
 * @param log output of log
 * @param size log size
 */
void mlog_port_output(const char* log, size_t size)
{
    /* add your code here */

    SEGGER_RTT_SetTerminal(1);
    SEGGER_RTT_WriteString(0, log);
}

/**
 * output lock
 */
void mlog_port_output_lock(void)
{
    /* add your code here */
}

/**
 * output unlock
 */
void mlog_port_output_unlock(void)
{
    /* add your code here */
}

/**
 * get current time interface
 *
 * @return current time
 */
const char* mlog_port_get_time(void)
{
    // char time_buf[20];
    // uint64_t tick = get_timer_tick();
    // uint32_t seconds = tick / 1000;
    // uint32_t milliseconds = tick % 1000;
    // snprintf(time_buf, sizeof(time_buf), "%lu.%03lu s", (unsigned long)seconds, (unsigned long)milliseconds);
    // return time_buf;
    return "";
    /* add your code here */
}

/**
 * get current process name interface
 *
 * @return current process name
 */
const char* mlog_port_get_p_info(void)
{
    return "";
    /* add your code here */
}

/**
 * get current thread name interface
 *
 * @return current thread name
 */
const char* mlog_port_get_t_info(void)
{
    return "";
    /* add your code here */
}
