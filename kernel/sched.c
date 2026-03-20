#include <errno.h>          /* 包含错误码定义 */
#include <linux/sched.h>    /* 进程调度相关，此处主要用于定义任务结构体和调度函数 */
#include <linux/sys.h>      // 包含系统调用相关定义，此处主要用于定义系统调用函数和系统调用表

#include <asm/system.h>
#include <asm/io.h> 

#define COUNTER 100     // 定义定时器的初始计数值，决定了定时器中断的频率，COUNTER越小，中断越频繁

#define LATCH (1193180/HZ)  /* 定义定时器的计数值，1193180是8253/8254定时器的输入频率，除以HZ得到每个时钟周期的计数值 */
#define PAGE_SIZE 4096  // 定义页面大小为4KB

extern int system_call();
extern void timer_interrupt();// 在汇编中定义了一个定时器中断处理函数，负责处理时钟中断，更新系统时间和进行任务调度
// 定义了一个任务联合体，PCB和内核栈共享同一内存空间，大小为一个页面（4KB）
union task_union {
    struct task_struct task;    // 任务结构体，包含了任务的LDT、TSS以及其他相关信息
    char stack[PAGE_SIZE];      // 任务的内核栈，大小为一个页面（4KB），用于存储任务切换时的上下文信息
};
// 静态分配大小为4K的初始PCB
static union task_union init_task = {INIT_TASK, };

struct task_struct * current = &(init_task.task);   // 定义一个指向当前任务的指针，初始指向init_task
struct task_struct * task[NR_TASKS] = {&(init_task.task), };    // 定义一个任务指针数组，包含了所有任务的指针，初始时只有init_task
// long类型的长度是4B    size = 4096 / 2 / 2
long user_stack[PAGE_SIZE >> 2];

struct
{
    long *a;    /*栈起始地址择子*/
    short b;    /*段选择子*/
} stack_start = {&user_stack[PAGE_SIZE >> 2], 0x10};


int clock = COUNTER;    // 全局变量，初始值为COUNTER，用于计数定时器中断的次数，决定了任务切换的频率

void do_timer(long cpl) {// 定时器中断处理函数，更新系统时间和进行任务调度
    if (clock >0 && clock <= COUNTER) {
        clock--;
    }
    else if (clock == 0) {
        schedule();
    }
    else {
        clock = COUNTER;
    }
}

void schedule() {   //实现简单的交替调度
    if (current == task[0] && task[1]) {
        switch_to(1);
    }
    else if (current == task[1]) {
        switch_to(0);
    }
}

void sched_init() {// 任务调度初始化函数，设置初始任务的TSS和LDT描述符，并将系统调用处理函数注册到IDT中
    int i = 0;
    struct desc_struct * p;
    set_tss_desc(gdt + FIRST_TSS_ENTRY, &(init_task.task.tss));// 设置初始任务的TSS描述符，指向init_task的TSS结构体
    set_ldt_desc(gdt + FIRST_LDT_ENTRY, &(init_task.task.ldt));// 设置初始任务的LDT描述符，指向init_task的LDT结构体
    
    p = gdt + FIRST_TSS_ENTRY + 2;  // 获取初始任务的之后的TSS描述符和GDT

    for(i = 1; i < NR_TASKS; i++) {
        task[i] = 0;    // 初始化任务数组,设置为0，表示没有其他任务
        p->a = p->b = 0;
        p++;
        p->a = p->b = 0;
        p++;
    }


    __asm__("pushfl; andl $0xffffbfff, (%esp); popfl"); // 清除EFLAGS寄存器中的NT标志，允许任务切换
    ltr(0);    // 加载初始任务的TSS到TR寄存器，准备进行任务切换
    lldt(0);    // 加载初始任务的LDT到LDTR寄存器，准备进行内存段切换
    
    /* open the clock interruption! */
    outb_p(0x36, 0x43);
    outb_p(LATCH & 0xff, 0x40);
    outb(LATCH >> 8, 0x40);
    set_intr_gate(0x20, &timer_interrupt);
    outb(inb_p(0x21) & ~0x01, 0x21);

    set_system_gate(0x80, &system_call);// 将系统调用处理函数注册到IDT的0x80号中断向量上，使用系统门（特权级3）
}

void test_a(void) { 
__asm__("movl $0x0, %edi\n\r"
        "movw $0x1b, %ax\n\t"
        "movw %ax, %gs \n\t"
        "movb $0x0c, %ah\n\r"
        "movb $'A', %al\n\r"
        "loopa:\n\r"
        "movw %ax, %gs:(%edi)\n\r"
        "jmp loopa");
}

void test_b(void) { 
__asm__("movl $0x30, %edi\n\r"
        "movw $0x1b, %ax\n\t"
        "movw %ax, %gs \n\t"
        "movb $0x0c, %ah\n\r"
        "movb $'B', %al\n\r"
        "loopb:\n\r"
        "movw %ax, %gs:(%edi)\n\r"
        "jmp loopb");
}
