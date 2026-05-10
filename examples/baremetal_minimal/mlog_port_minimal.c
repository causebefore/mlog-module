#include <stddef.h>
#include <stdint.h>

#include <mlog.h>

#if defined(__GNUC__) || defined(__clang__)
    #define MLOG_WEAK __attribute__((weak))
#elif defined(__ICCARM__) || defined(__CC_ARM) || defined(__ARMCC_VERSION)
    #define MLOG_WEAK __weak
#else
    #define MLOG_WEAK
#endif

/*
 * 用户可在应用层实现该函数，将日志输出到 UART/SWO/RTT 等接口。
 * 默认弱实现为空，保证模块可先集成后替换。
 * 该函数返回前必须已经消费或复制 log；若使用 DMA/队列等异步输出，
 * 应在移植层自备缓冲区，不能长期保存 log 指针。
 */
MLOG_WEAK void mlog_port_hw_write(const char* log, size_t size)
{
    (void) log;
    (void) size;
}

/*
 * 可选中断保护钩子：
 * 1) 默认空实现，保证工程初始化后可直接编译；
 * 2) 用户可在平台层重写为保存/恢复 PRIMASK 或进入/退出临界区。
 */
MLOG_WEAK uint32_t mlog_port_irq_save(void)
{
    return 0U;
}

MLOG_WEAK void mlog_port_irq_restore(uint32_t state)
{
    (void) state;
}

static uint32_t s_primask = 0U;

static MlogErrCode port_init(void)
{
    return MLOG_NO_ERR;
}

static void port_deinit(void)
{
}

static void port_output(const char* log, size_t size)
{
    mlog_port_hw_write(log, size);
}

static void port_output_lock(void)
{
    s_primask = mlog_port_irq_save();
}

static void port_output_unlock(void)
{
    mlog_port_irq_restore(s_primask);
}

static const char* port_get_time(void)
{
    return "";
}

static const char* port_get_p_info(void)
{
    return "";
}

static const char* port_get_t_info(void)
{
    return "";
}

static const MlogPortInterface s_default_port = {
    .init          = port_init,
    .deinit        = port_deinit,
    .output        = port_output,
    .output_lock   = port_output_lock,
    .output_unlock = port_output_unlock,
    .get_time      = port_get_time,
    .get_p_info    = port_get_p_info,
    .get_t_info    = port_get_t_info,
};

const MlogPortInterface* mlog_port_get_default(void)
{
    return &s_default_port;
}
