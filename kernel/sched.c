#include <linux/sched.h>

#include <asm/system.h>

#define PAGE_SIZE 4096
/// @brief 函数实现在汇编级代码中，目前只是打印输出内容 @return 
extern int system_call();
// long类型的长度是4B    size = 4096 / 2 / 2
long user_stack[PAGE_SIZE >> 2];

struct
{
    long *a;    /*栈起始地址择子*/
    short b;    /*段选择子*/
} stack_start = {&user_stack[PAGE_SIZE >> 2], 0x10};

void sched_init() {
    set_intr_gate(0x80, &system_call);
}
