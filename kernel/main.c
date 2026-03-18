#define __LIBRARY__ /* 定义库标志，用于条件编译，通常表示正在构建内核库 */

#include <linux/tty.h>      /* 终端设备相关头文件 */
#include <linux/kernel.h>   /* 内核通用函数 */
#include <linux/sched.h>

extern void mem_init(long start, long end); /* 内存初始化函数声明，在mm/memory.c中定义 */

#define EXT_MEM_K (*(unsigned short *)0x90002)  /* 从物理地址0x90002读取扩展内存大小（以KB为单位），该值由引导程序setup.s获取并存放 */

static long memory_end = 0;             /* 物理内存总大小（字节），最终将确定 */
static long buffer_memory_end = 0;      /* 缓冲区内存的结束地址，用于块设备缓存 */
static long main_memory_start = 0;      /* 主内存区域的起始地址（供用户进程使用） */

void main(void) {
    memory_end = (1<<20) + (EXT_MEM_K<<10);     /* 计算物理内存空间大小*/
    memory_end &= 0xfffff000;                   /* 将内存大小对齐到4KB边界（页大小） */
    if (memory_end > 16*1024*1024)                  /* 限制最大内存为16MB（Linux 0.12内核设计上限） */
        memory_end = 16*1024*1024;              
    if (memory_end > 12*1024*1024)          /* 根据内存总量决定缓冲区（buffer）的大小，缓冲区用于磁盘高速缓存 */
        buffer_memory_end = 4*1024*1024;        /* 如果内存 > 12MB，则缓冲区设为4MB */
    else if (memory_end > 6*1024*1024)          
        buffer_memory_end = 2*1024*1024;        /* 如果内存 > 6MB但≤12MB，缓冲区设为2MB */
    else
        buffer_memory_end = 1*1024*1024;        /* 否则（≤6MB），缓冲区设为1MB */

    main_memory_start = buffer_memory_end;      /* 主内存起始地址即为缓冲区的结束地址，主内存区域将用于动态分配（如用户进程） */
    mem_init(main_memory_start, memory_end);    /* 初始化内存管理，设置主内存区域的起始和结束地址 */
    
    trap_init();
    sched_init();

    tty_init();
    
    printk("memory start: %d, end: %d\n", main_memory_start, memory_end);

    __asm__ __volatile__(
            "int $0x7f\n\r"
            "int $0x80\n\r"
            "movw $0x1b, %%ax\n\r"
            "movw %%ax, %%gs\n\r"
            "movl $0, %%edi\n\r"
            "movw $0x0f41, %%gs:(%%edi)\n\r"
            "loop:\n\r"
            "jmp loop"
            ::);
}
