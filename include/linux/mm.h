#ifndef _MM_H
#define _MM_H

#define PAGE_SIZE 4096  // 定义页面大小为4KB

extern unsigned long get_free_page();// 获取一个空闲页框的函数，返回页框的物理地址

#define LOW_MEM 0x100000                    // 内核可用内存的起始地址，1MB
extern unsigned long HIGH_MEMORY;           // 内核可用内存的最高地址
#define PAGING_MEMORY (15*1024*1024)        // 15MB的内存空间
#define PAGING_PAGES (PAGING_MEMORY>>12)    // 15MB的内存空间，除以4KB得到页数
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12) // 计算地址addr对应的页框号
#define USED  100                   // 已使用页框的标志

extern unsigned char mem_map [ PAGING_PAGES ];  // 内核使用的内存映射表，记录每个页框的使用状态，大小为PAGING_PAGES

#endif

