#include <errno.h>

#include <asm/system.h>
#include <linux/sched.h>
//寻找一个空的任务槽，返回其索引，如果没有空闲槽则返回错误码
int find_empty_process() {
    int i;
    for (i = 1; i < NR_TASKS; i++) {
        if (!task[i])
            return i;
    }

    return -EAGAIN;
}

