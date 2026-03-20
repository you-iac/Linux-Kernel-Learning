#ifndef _SCHED_H    // 任务调度相关的头文件
#define _SCHED_H

#define HZ 100  /* 定义系统时钟频率为100Hz，即每秒钟产生100次时钟中断 */

#define NR_TASKS 64  // 定义系统中最大任务数为64
#define TASK_SIZE       0x04000000

#if (TASK_SIZE & 0x3fffff)
#error "TASK_SIZE must be multiple of 4M"
#endif

#if (((TASK_SIZE>>16)*NR_TASKS) != 0x10000)
#error "TASK_SIZE*NR_TASKS must be 4GB"
#endif

#define FIRST_TASK task[0]  // 定义第一个任务为任务指针数组中的第一个元素，即init_task
#define LAST_TASK task[NR_TASKS-1]  // 定义最后一个任务为任务指针数组中的最后一个元素，即task[63]

#include <linux/head.h>
#include <linux/mm.h>

#define TASK_RUNNING            0
#define TASK_INTERRUPTIBLE      1
#define TASK_UNINTERRUPTIBLE    2
#define TASK_ZOMBIE             3
#define TASK_STOPPED            4

#ifndef NULL
#define NULL ((void *) 0)
#endif

extern int copy_page_tables(unsigned long from, unsigned long to, long size);
extern int free_page_tables(unsigned long from, unsigned long size);

extern void trap_init();
extern void sched_init();
extern void schedule();
extern void panic(const char* s);
void test_a();
void test_b();

extern struct task_struct *task[NR_TASKS];  // 定义一个任务指针数组，包含了所有任务的指针，初始时只有init_task
extern struct task_struct *current;     // 定义一个指向当前任务的指针

typedef int (*fn_ptr)();
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
    long state;
    long pid;
    struct task_struct      *p_pptr;
    struct desc_struct ldt[3];
    struct tss_struct tss;
};
// 定义了一个可填充的初始任务结构体的数据体
#define INIT_TASK \ 
{                   \
    0,              \
    0,              \
    &init_task.task,\
    {               \
        {0, 0},     /* LDT[0]: 空描述符 */ \
        {0xfff, 0xc0fa00},   /* LDT[1]: 代码段描述符 (0x9f: 类型和权限, 0xc0fa00: 基地址和限长) */ \
        {0xfff, 0xc0f200},   /* LDT[2]: 数据段描述符 (0x9f: 类型和权限, 0xc0f200: 基地址和限长) */ \
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

#define switch_to(n) {  /*实现任务切换*/ \
    struct {long a,b;} __tmp; \
    __asm__("cmpl %%ecx,current\n\t" \
            "je 1f\n\t" \
            "movw %%dx,%1\n\t" \
            "xchgl %%ecx,current\n\t" \
            "ljmp *%0\n\t" \
            "1:" \
            ::"m" (*&__tmp.a),"m" (*&__tmp.b), \
            "d" (_TSS(n)),"c" ((long) task[n])); \
}  

#define _set_base(addr,base) \
__asm__("movw %%dx,%0\n\t" \
        "rorl $16,%%edx\n\t" \
        "movb %%dl,%1\n\t" \
        "movb %%dh,%2" \
        ::"m" (*((addr)+2)), \
        "m" (*((addr)+4)), \
        "m" (*((addr)+7)), \
        "d" (base) \
        :)

#define _set_limit(addr,limit) \
__asm__("movw %%dx,%0\n\t" \
        "rorl $16,%%edx\n\t" \
        "movb %1,%%dh\n\t" \
        "andb $0xf0,%%dh\n\t" \
        "orb %%dh,%%dl\n\t" \
        "movb %%dl,%1" \
        ::"m" (*(addr)), \
        "m" (*((addr)+6)), \
        "d" (limit) \
        :"dx")

#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , base )
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )

#define _get_base(addr) ({\
unsigned long __base; \
__asm__("movb %3,%%dh\n\t" \
    "movb %2,%%dl\n\t" \
    "shll $16,%%edx\n\t" \
    "movw %1,%%dx" \
    :"=d" (__base) \
    :"m" (*((addr)+2)), \
    "m" (*((addr)+4)), \
    "m" (*((addr)+7))); \
__base;})

#define get_base(ldt) _get_base( ((char *)&(ldt)) )

#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit;})

#endif

