#include <stdint.h>

#define LOG_TAG "app"
#define LOG_LVL MLOG_LVL_DEBUG
#include <mlog.h>

int main(void)
{
    /* 注册移植层接口（在 mlog_init 前调用） */
    mlog_port_register(mlog_port_get_default());

    if (mlog_init() != MLOG_NO_ERR)
    {
        while (1)
        {
            /* 初始化失败时停机，避免继续运行到未知状态 */
        }
    }

    mlog_start();

    log_i("mlog minimal example start");

    for (uint32_t tick = 0U; tick < 3U; tick++)
    {
        log_d("tick=%lu", (unsigned long) tick);
    }

    mlog_stop();
    mlog_deinit();

    while (1)
    {
        /* 裸机环境主循环 */
    }
}
