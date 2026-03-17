#ifndef _SYSTEM_H
#define _SYSTEM_H

#define sti()   __asm__("sti"::)    //启用中断
#define cli()   __asm__("cli"::)    //禁用中断
#define nop()   __asm__("nop"::)    //执行空操作
#define iret()  __asm__("iret"::)   //从中断返回
        //把中断处理函数的地址 填入指定的内存地址
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

        //把第 n 号中断，绑定到地址为 addr 的处理函数
#define set_intr_gate(n, addr) \
    _set_gate(&idt[n], 14, 0, addr)

#endif

