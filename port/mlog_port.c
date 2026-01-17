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

#include "shell.h"
#include "usart.h"

#include <mlog.h>

/*===========================================================================*/
/* Default port callback implementations                                      */
/* Users can modify these or create new ones and register via                 */
/* mlog_port_register() before calling mlog_init()                            */
/*===========================================================================*/

static MlogErrCode default_port_init(void)
{
    MlogErrCode result = MLOG_NO_ERR;
    /* add your code here */
    return result;
}

static void default_port_deinit(void)
{
    /* add your code here */
}

static void default_port_output(const char* log, size_t size)
{
    shell_log(log, (int) size);
}

/* 用于保护log_buf的中断状态 */
static uint32_t s_mlog_primask = 0;

static void default_port_output_lock(void)
{
    s_mlog_primask = __get_PRIMASK();
    __disable_irq();
}

static void default_port_output_unlock(void)
{
    __set_PRIMASK(s_mlog_primask);
}

static const char* default_port_get_time(void)
{
    return "";
    /* add your code here */
}

static const char* default_port_get_p_info(void)
{
    return "";
    /* add your code here */
}

static const char* default_port_get_t_info(void)
{
    return "";
    /* add your code here */
}

/*===========================================================================*/
/* Default port interface - register this before mlog_init()                  */
/*===========================================================================*/

static const MlogPortInterface s_default_port = {
    .init          = default_port_init,
    .deinit        = default_port_deinit,
    .output        = default_port_output,
    .output_lock   = default_port_output_lock,
    .output_unlock = default_port_output_unlock,
    .get_time      = default_port_get_time,
    .get_p_info    = default_port_get_p_info,
    .get_t_info    = default_port_get_t_info,
};

/**
 * Get default port interface
 * Call mlog_port_register(&mlog_port_get_default()) before mlog_init()
 *
 * @return pointer to default port interface
 */
const MlogPortInterface* mlog_port_get_default(void)
{
    return &s_default_port;
}
