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

void schedule() {   //实现进程调度
    int i,next,c;                    // i:循环变量, next:下次要切换的任务号, c:最大 counter
    struct task_struct ** p;         // 指向 task 数组的指针

    while(1) {                       // 选择下一个要运行的任务
        c = -1;                       // 当前最大 counter 初始化为 -1
        next = 0;                     // 默认切换到 init(0)
        i = NR_TASKS;                 // 从任务表末尾开始扫描
        p = &task[NR_TASKS];

        while (--i) {                 // 遍历所有任务槽
            if (!*--p)                // 空槽跳过
                continue;

            if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
                c = (*p)->counter, next = i; // 选择可运行且 counter 最大的任务
        }

        if (c) break;                 // 找到合适任务则退出循环
        for(p = &LAST_TASK ; p > &FIRST_TASK ; --p) { // 否则重算各任务的 counter
            if (!(*p))                // 空槽跳过
                continue;

            (*p)->counter = ((*p)->counter >> 1) + (*p)->priority; // 衰减计数并加上优先级
        }
    }
    switch_to(next);                 // 执行任务切换到 next
}
// 内部函数，封装了可中断和不可中断的睡眠操作，参数 state 决定了睡眠的类型
static inline void __sleep_on(struct task_struct** p, int state) {
    struct task_struct* tmp;    //内核栈申请空间，并且作为下一个唤醒的地址，实现唤醒队列的静态链表设计

    if (!p)
        return;
    if (current == &(init_task.task))
        panic("task[0] trying to sleep");

    tmp = *p;
    *p = current;
    current->state = state;

repeat:
    schedule();

    if (*p && *p != current) {
        (**p).state = 0;
        current->state = TASK_UNINTERRUPTIBLE;
        goto repeat;
    }

    if (!*p)
        printk("Warning: *P = NULL\n\r");
    *p = tmp;
    if (*p)
        tmp->state = 0;
}
// 可中断睡眠，允许被信号中断唤醒
void interruptible_sleep_on(struct task_struct** p) {
    __sleep_on(p, TASK_INTERRUPTIBLE);
}
// 不可中断睡眠，不能被信号中断唤醒
void sleep_on(struct task_struct** p) {
    __sleep_on(p, TASK_UNINTERRUPTIBLE);
}
// 唤醒一个睡眠的任务，参数 p 是指向任务指针的指针，唤醒后将任务状态设置为可运行
void wake_up(struct task_struct **p) {
    if (p && *p) {
        if ((**p).state == TASK_STOPPED)
            printk("wake_up: TASK_STOPPED");
        if ((**p).state == TASK_ZOMBIE)
            printk("wake_up: TASK_ZOMBIE");
        (**p).state=0;
    }
}

void do_timer(long cpl) {
    if ((--current->counter)>0) return;
    current->counter=0;
    if (!cpl) return;
    schedule();
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
__asm__("movl $0x0, %edi\n\r"
        "movw $0x1b, %ax\n\t"
        "movw %ax, %gs \n\t"
        "movb $0x0f, %ah\n\r"
        "movb $'B', %al\n\r"
        "loopb:\n\r"
        "movw %ax, %gs:(%edi)\n\r"
        "jmp loopb");
}
