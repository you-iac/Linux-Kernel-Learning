#include <linux/sched.h>    /* 包含进程调度相关的定义，如 PAGING_PAGES、MAP_NR、USED 等宏 */

unsigned long HIGH_MEMORY = 0;/* 记录物理内存的最高地址（字节），将在 mem_init 中初始化为 end_mem */

unsigned char mem_map [ PAGING_PAGES ] = {0,};/* mem_map 是物理内存页框的使用状态数组 每个元素对应一个物理页*/

/// @brief 初始化物理内存页框管理 @param start_mem 主内存区域的起始物理地址s @param end_mem 物理内存的结束地址（最高地址）
void mem_init(long start_mem, long end_mem) {
    int i;

    HIGH_MEMORY = end_mem;  /* 保存物理内存最高地址 */
    
    for (i = 0; i < PAGING_PAGES; i++) {    /* 初始时，假设所有物理页框都已被占用（USED 为非零值） */
        mem_map[i] = USED;
    }
    /* 计算主内存起始地址对应的页框号（物理地址右移12位） */
    i = MAP_NR(start_mem);  /* 主内存区域字节数 */
    end_mem -= start_mem;   /* 主内存区域页框数 */
    end_mem >>= 12;
    while (end_mem--) { /* 将主内存区域对应的页框标记为空闲（0） */
        mem_map[i++] = 0;
    }

}

