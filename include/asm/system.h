#ifndef _SYSTEM_H /* 基础 CPU 控制宏 */
#define _SYSTEM_H

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
/// 陷阱门：主要用于 CPU 内部产生的异常（如除零、溢出） @param n 中断向量号 @param addr 处理函数的地址
#define set_trap_gate(n, addr) \
    _set_gate(&idt[n], 15, 0, addr)        
//  中断门：主要用于外部硬件中断（如时钟、键盘） @param n 中断向量号 @param addr 处理函数的地址
#define set_intr_gate(n, addr) \
    _set_gate(&idt[n], 14, 0, addr)
//@brief 系统门：主要用于系统调用等需要用户态访问的特殊功能 @param n 中断向量号 @param addr 处理函数的地址
#define set_system_gate(n, addr) \
    _set_gate(&idt[n], 15, 3, addr)  
/// @brief 设置TSS或LDT描述符的宏函数，n是描述符表项地址，addr是TSS或LDT结构体的地址，type是描述符类型（0x89表示TSS，0x82表示LDT）
#define _set_tssldt_desc(n, addr, type) \  
__asm__("movw $104, %1\n\t"             \
        "movw %%ax, %2\n\t"             \
        "rorl $16, %%eax\n\t"           \
        "movb %%al, %3\n\t"             \
        "movb $" type ", %4\n\t"        \
        "movb $0x00, %5\n\t"            \
        "movb %%ah, %6\n\t"             \
        "rorl $16, %%eax\n\t"           \
        ::"a"(addr), "m"(*(n)), "m"(*(n+2)), "m"(*(n+4)), \
        "m"(*(n+5)), "m"(*(n+6)), "m"(*(n+7)) \
       )

#define set_tss_desc(n, addr) _set_tssldt_desc(((char*)(n)), addr, "0x89")  // 设置TSS描述符
#define set_ldt_desc(n, addr) _set_tssldt_desc(((char*)(n)), addr, "0x82")  // 设置LDT描述符

#endif

