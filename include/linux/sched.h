#ifndef _SCHED_H    // 任务调度相关的头文件
#define _SCHED_H

#include <linux/head.h>
#include <linux/mm.h>
//初始化任务调度
void sched_init();
// 任务状态段（TSS）结构体，包含了任务切换时需要保存的寄存器状态和其他相关信息
struct tss_struct {
    long back_link;      // 后向链接，用于任务切换
    long esp0;           // 特权级0的栈指针
    long ss0;            // 特权级0的栈段选择子
    long esp1;           // 特权级1的栈指针
    long ss1;            // 特权级1的栈段选择子
    long esp2;           // 特权级2的栈指针
    long ss2;            // 特权级2的栈段选择子
    long cr3;            // 页目录基地址
    long eip;            // 指令指针
    long eflags;         // 标志寄存器
    long eax, ecx, edx, ebx;  // 通用寄存器
    long esp;            // 栈指针
    long ebp;            // 基指针
    long esi;            // 源索引寄存器
    long edi;            // 目的索引寄存器
    long es;             // ES段选择子
    long cs;             // CS段选择子
    long ss;             // SS段选择子
    long ds;             // DS段选择子
    long fs;             // FS段选择子
    long gs;             // GS段选择子
    long ldt;            // LDT选择子
    long trace_bitmap;   // 调试跟踪位图
};
/// 任务结构体，包含了任务的LDT、TSS以及其他相关信息
struct task_struct {
    struct desc_struct ldt[3];
    struct tss_struct tss;
};
// 定义了一个可填充的初始任务结构体的数据体
#define INIT_TASK \ 
{                   \
    {               \
        {0, 0},     \
        {0x9f, 0xc0fa00},   \
        {0x9f, 0xc0f200},   \
    },              \
    {0, PAGE_SIZE + (long)&init_task, 0x10, 0, 0, 0, 0, (long)&pg_dir, \
        0, 0, 0, 0, 0, 0, 0, 0, \
        0, 0, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,   \
        _LDT(0), 0x80000000,    \
    },              \
}

/*
 * In linux is 4, because we add video selector,
 * so, it is 5 here.
 * */
#define FIRST_TSS_ENTRY 5                       // 第一个TSS条目在GDT中的索引
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY + 1)   // 第一个LDT条目在GDT中的索引
#define _TSS(n) ((((unsigned long)n) << 4) + (FIRST_TSS_ENTRY << 3))  // 计算任务n的TSS选择子
#define _LDT(n) ((((unsigned long)n) << 4) + (FIRST_LDT_ENTRY << 3))  // 计算任务n的LDT选择子
#define ltr(n) __asm__("ltr %%ax"::"a"(_TSS(n)))    // 加载任务n的TSS到TR寄存器
#define lldt(n) __asm__("lldt %%ax"::"a"(_LDT(n)))  // 加载任务n的LDT到LDTR寄存器

#endif

