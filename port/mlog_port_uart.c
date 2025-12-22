/*
 * @Author: liu
 * @Date: 2025-12-09 14:59:19
 * @LastEditors: liu lbq08@foxmail.com
 * @LastEditTime: 2025-12-09 15:33:12
 * @FilePath: \Mlog\port\mlog_port copy.c
 * @Description: mlog的串口移植层代码文件，用户需根据具体平台实现相关接口
 *
 * Copyright (c) 2025 by liu lbq08@foxmail.com, All Rights Reserved.
 */

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
void mlog_port_output(const char *log, size_t size)
{
    /* add your code here */
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
const char *mlog_port_get_time(void)
{
    /* add your code here */
}

/**
 * get current process name interface
 *
 * @return current process name
 */
const char *mlog_port_get_p_info(void)
{
    /* add your code here */
}

/**
 * get current thread name interface
 *
 * @return current thread name
 */
const char *mlog_port_get_t_info(void)
{
    /* add your code here */
}