#include <asm/system.h>  /* 包含 _set_gate, set_trap_gate 等宏定义 */
#include <linux/kernel.h>/* 包含 printk 等内核函数 */
#include <linux/sched.h> /* 进程调度相关，此处主要用于 printk 等内核函数 */
#include <asm/io.h>      /* 包含 inb_p, outb_p 等端口 I/O 宏 */

#define get_seg_byte(seg,addr) ({ \
register char __res; \
__asm__("push %%fs;mov %%ax,%%fs;movb %%fs:%2,%%al;pop %%fs" \
 :"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})

#define get_seg_long(seg,addr) ({ \
register unsigned long __res; \
__asm__("push %%fs;mov %%ax,%%fs;movl %%fs:%2,%%eax;pop %%fs" \
    :"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})


#define _fs() ({ \
register unsigned short __res; \
__asm__("mov %%fs,%%ax":"=a" (__res):); \
__res;})

/* 以下是汇编语言实现的中断处理跳转入口（在 asm.s 中定义） */
void divide_error();      // 0号：除零错误
void debug();             // 1号：调试断点
void nmi();               // 2号：非屏蔽中断
void int3();              // 3号：断点指令 (int 3)
void overflow();          // 4号：溢出
void bounds();            // 5号：边界检查失败
void invalid_op();        // 6号：无效指令
void double_fault();      // 8号：双重错误
void coprocessor_segment_overrun(); // 9号：协处理器段越界
void invalid_TSS();       // 10号：无效的任务状态段
void segment_not_present(); // 11号：段不存在
void stack_segment();     // 12号：堆栈段错误
void page_fault();      // 14号：页面错误（缺页或访问权限错误）
void general_protection(); // 13号：一般保护性异常（最常遇到的权限问题）
void reserved();          // 预留中断入口
void irq13();             // 协处理器中断
void alignment_check();   // 17号：对齐检查

static void die(char* str, long esp_ptr, long nr) {/* 打印错误信息并进入死循环（内核恐慌的一种简易实现） */
    int i = 0;    
    long* esp = (long*)esp_ptr;

    printk("\n\r%s: %04x\n\r", str, nr & 0xffff);
    printk("EIP:\t%04x:%p\n\rEFLAGS:\t%p\n\rESP:\t%04x:%p\n\r",
            esp[1],esp[0],esp[2],esp[4],esp[3]);

    printk("fs: %04x\n\r",_fs());
    printk("base: %p, limit: %p\n\r",get_base(current->ldt[1]),get_limit(0x17));
    if (esp[4] == 0x17) {
        printk("Stack: ");
        for (i=0;i<4;i++)
            printk("%p ",get_seg_long(0x17,i+(long *)esp[3]));
        printk("\n\r");
    }

    for(i=0;i<10;i++)
        printk("%02x ",0xff & get_seg_byte(esp[1],(i+(char *)esp[0])));
    printk("\n\r");

    while (1) { // 发生这种异常时，内核无法继续运行，直接挂起
    }
}

void do_double_fault(long esp, long error_code) {/* 各种 do_ 开头的函数是由汇编跳转入口调用的 C 处理逻辑 */
    die("double fault", esp, error_code);
}

void do_general_protection(long esp, long error_code) {
    die("general protection", esp, error_code);
}

void do_alignment_check(long esp, long error_code) {
    die("alignment check", esp, error_code);
}

void do_divide_error(long esp, long error_code) {
    die("divide error", esp, error_code);
}

void do_int3(long * esp, long error_code,/* int3 是特殊的系统门，用于调试，它会打印当前所有寄存器的状态 */
        long fs,long es,long ds,
        long ebp,long esi,long edi,
        long edx,long ecx,long ebx,long eax) {
    int tr;

    __asm__("str %%ax":"=a" (tr):"" (0)); // 读取任务寄存器 TR
    printk("eax\t\tebx\t\tecx\t\tedx\n\r%8x\t%8x\t%8x\t%8x\n\r",
            eax,ebx,ecx,edx);
    printk("esi\t\tedi\t\tebp\t\tesp\n\r%8x\t%8x\t%8x\t%8x\n\r",
            esi,edi,ebp,(long) esp);
    printk("\n\rds\tes\tfs\ttr\n\r%4x\t%4x\t%4x\t%4x\n\r",
            ds,es,fs,tr);
    printk("EIP: %8x   CS: %4x  EFLAGS: %8x\n\r",esp[0],esp[1],esp[2]);
}

void do_nmi(long esp, long error_code) {
    die("nmi", esp, error_code);
}

void do_debug(long esp, long error_code) {
    die("debug", esp, error_code);
}

void do_overflow(long esp, long error_code) {
    die("overflow", esp, error_code);
}

void do_bounds(long esp, long error_code) {
    die("bounds", esp, error_code);
}

void do_invalid_op(long esp, long error_code) {
    die("invalid_op", esp, error_code);
}

void do_device_not_available(long esp, long error_code) {
    die("device not available", esp, error_code);
}

void do_coprocessor_segment_overrun(long esp, long error_code) {
    die("coprocessor segment overrun", esp, error_code);
}

void do_segment_not_present(long esp, long error_code) {
    die("segment not present", esp, error_code);
}

void do_invalid_TSS(long esp, long error_code) {
    die("invalid tss", esp, error_code);
}

void do_stack_segment(long esp, long error_code) {
    die("stack segment", esp, error_code);
}

void do_reserved(long esp, long error_code) {
    die("reserved (15,17-47) error",esp,error_code);
}
/** trap_init: 在 main.c 中被调用。 作用是初始化 IDT 表，将硬件异常与对应的汇编入口绑定。*/
void trap_init() {
    int i;
                            /* 设置陷阱门 (Type 15, DPL 0): 只有内核能触发，进入时不关中断 */
    set_trap_gate(0, &divide_error);
    set_trap_gate(1,&debug);
    set_trap_gate(2,&nmi);
    set_system_gate(3,&int3);
    set_system_gate(4,&overflow);
    set_system_gate(5,&bounds);
    set_trap_gate(6,&invalid_op);
    set_trap_gate(8,&double_fault);
    set_trap_gate(9,&coprocessor_segment_overrun);
    set_trap_gate(10, &invalid_TSS);
    set_trap_gate(11, &segment_not_present);
    set_trap_gate(12, &stack_segment);
    set_trap_gate(14, &page_fault);
    set_trap_gate(13, &general_protection);
    set_trap_gate(15,&reserved);
    set_trap_gate(17,&alignment_check);
                            /* 将 18-47 号未使用的中断项全部指向预留处理函数 */
    for (i=18;i<48;i++)
        set_trap_gate(i,&reserved);
                            /* 允许 8259A 中断控制器的从片（Slave）级联中断信号进入 */
    outb_p(inb_p(0x21)&0xfb,0x21);  // 复位主片上的 IRQ2（对应从片级联）
    outb(inb_p(0xA1)&0xdf,0xA1);    // 开启从片上的 IRQ5（协处理器）
}