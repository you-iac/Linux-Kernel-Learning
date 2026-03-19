#include <linux/sched.h>

#include <asm/system.h>

#define PAGE_SIZE 4096  // 定义页面大小为4KB
// 定义了一个初始任务（init_task），作为系统启动时的第一个任务
extern int system_call();
// 定义了一个任务联合体，PCB和内核栈共享同一内存空间，大小为一个页面（4KB）
union task_union {
    struct task_struct task;    // 任务结构体，包含了任务的LDT、TSS以及其他相关信息
    char stack[PAGE_SIZE];      // 任务的内核栈，大小为一个页面（4KB），用于存储任务切换时的上下文信息
};
// 静态分配大小为4K的初始PCB
static union task_union init_task = {INIT_TASK, };
// long类型的长度是4B    size = 4096 / 2 / 2
long user_stack[PAGE_SIZE >> 2];

struct
{
    long *a;    /*栈起始地址择子*/
    short b;    /*段选择子*/
} stack_start = {&user_stack[PAGE_SIZE >> 2], 0x10};
// 任务调度初始化函数，设置初始任务的TSS和LDT描述符，并将系统调用处理函数注册到IDT中
void sched_init() {
    struct desc_struct* p;
    set_tss_desc(gdt + FIRST_TSS_ENTRY, &(init_task.task.tss));// 设置初始任务的TSS描述符，指向init_task的TSS结构体
    set_ldt_desc(gdt + FIRST_LDT_ENTRY, &(init_task.task.ldt));// 设置初始任务的LDT描述符，指向init_task的LDT结构体
    
    __asm__("pushfl; andl $0xffffbfff, (%esp); popfl"); // 清除EFLAGS寄存器中的NT标志，允许任务切换
    ltr(0);    // 加载初始任务的TSS到TR寄存器，准备进行任务切换
    lldt(0);    // 加载初始任务的LDT到LDTR寄存器，准备进行内存段切换
    set_system_gate(0x80, &system_call);// 将系统调用处理函数注册到IDT的0x80号中断向量上，使用系统门（特权级3）
}

