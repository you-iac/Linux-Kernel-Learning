#ifndef _SYSTEM_H
#define _SYSTEM_H
/** 基础 CPU 控制宏*/
#define sti()   __asm__("sti"::)    //启用中断
#define cli()   __asm__("cli"::)    //禁用中断
#define nop()   __asm__("nop"::)    //执行空操作
#define iret()  __asm__("iret"::)   //从中断返回
//把中断处理函数的地址 填入指定的内存地址@param gate_addr IDT的地址 @param type 14 关中断 15 不关中断 @param dpl 操作权限 @param addr 处理函数的地址
#define _set_gate(gate_addr, type, dpl, addr) \
__asm__("movw %%dx, %%ax\n\t"   \
        "movw %0, %%dx\n\t"     \
        "movl %%eax, %1\n\t"    \
        "movl %%edx, %2"        \
        :                       \
        :"i"((short)(0x8000 + (dpl << 13) + (type << 8))),  \
        "o"(*((char*)(gate_addr))),                         \
        "o"(*(4 + (char*)(gate_addr))),                     \
        "d"((char*)(addr)), "a" (0x00080000))
/// 陷阱门：主要用于 CPU 内部产生的异常（如除零、溢出）
#define set_trap_gate(n, addr) \
    _set_gate(&idt[n], 15, 0, addr)        
//  中断门：主要用于外部硬件中断（如时钟、键盘）
#define set_intr_gate(n, addr) \
    _set_gate(&idt[n], 14, 0, addr)
//  系统门：专门用于用户态调用的中断
#define set_system_gate(n, addr) \
    _set_gate(&idt[n], 15, 3, addr)    
#endif

