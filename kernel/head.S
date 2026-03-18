.code32             /* 使用32位模式 */
.text               /* 代码段 */
.globl startup_32, idt, gdt, pg_dir, tmp_floppy_area  /* 导出全局符号 */
pg_dir:             /* 页目录起始位置 相对位置为0*/
startup_32:         /* 32位启动入口 */
    movl $0x10, %eax    /* 初始化段寄存器为内核数据段选择子 (0x10) */
    movw %ax,   %ds
    movw %ax,   %es
    movw %ax,   %fs
    movw %ax,   %gs

    lss  stack_start, %esp  /* 设置内核堆栈，stack_start 在外部定义（指向栈顶） */ 
    call setup_idt          /* 调用中断描述符表(IDT)设置函数 */
    call setup_gdt          /* 调用全局描述符表(GDT)设置函数 */
    
    movl $0x10, %eax    /* 重新加载段寄存器，因为 GDT 可能已改变 */
    movw %ax,   %ds
    movw %ax,   %es
    movw %ax,   %fs

    movl $0x18, %eax    /* 设置 GS 段为 0x18（通常用于显存段），并向显存写入字符 'B' (0x42) 属性 0xf（白底高亮？） */
    movw %ax, %gs
                    /* 以下循环检测 A20 地址线是否开启：向地址 0 写入值，然后检查 1MB 地址的值是否相同，如果相同说明 A20 未开启（1MB 地址被别名到 0），则死循环；否则继续执行 */
    xorl %eax, %eax      /* eax = 0 */
1:
    incl %eax            /* eax += 1 */
    movl %eax, 0x000000  /* 将 eax 写入内存地址 0 */
    cmpl %eax, 0x100000  /* 比较 1MB 地址的值是否等于 eax */
    je   1b              /* 如果相等，则跳回 1 继续测试（实际会无限循环，直到 A20 开启） */
    

    jmp  after_page_tables  /* A20 已开启，跳转到分页和 main 函数的准备代码 */

/* 设置中断描述符表：所有 256 个中断门都指向 ignore_int 处理函数 */
setup_idt:
    leal ignore_int, %edx    /* edx = ignore_int 函数的地址 */
    movl $0x00080000, %eax   /* 段选择子 0x08（内核代码段）放在高16位 */
    movw %dx, %ax            /* 将 ignore_int 的低16位放入 ax，形成中断门低32位 */
    movw $0x8e00, %dx        /* 中断门标志：present, DPL=0, 386中断门，并将 edx 的高16位设为 0x8e00？实际上是设置高16位为 0x8e00，低16位为 ignore_int 的高16位 */
    leal idt, %edi           /* edi 指向 idt 表起始 */
    movl $256, %ecx          /* 设置 256 个中断门 */
rp_sidt:
    movl %eax, (%edi)        /* 写入中断门低32位 */
    movl %edx, 4(%edi)       /* 写入中断门高32位 */
    addl $8, %edi            /* 指向下一个中断门（8字节） */
    decl %ecx
    jne  rp_sidt
    lidt idt_descr           /* 加载 IDTR 寄存器 */
    ret
                    /* 设置全局描述符表：加载 GDTR 寄存器 */
setup_gdt:
    lgdt gdt_descr
    ret

.org 0x1000          /* 从当前位置跳转到 0x1000 偏移处 */
pg0:                 /* 页表 0 起始位置（占用 4KB） */

.org 0x2000
pg1:                 /* 页表 1 起始位置 */

.org 0x3000
pg2:                 /* 页表 2 起始位置 */

.org 0x4000
pg3:                 /* 页表 3 起始位置 */

.org 0x5000          /*后续代码执行位置*/

tmp_floppy_area:     /* 软盘临时缓冲区（1024字节，初始化为0） */
.fill 1024, 1, 0

after_page_tables:  /* 准备跳转到 main 函数 */
    /* 为 main 函数压栈参数（这里 main 原型是 void main(void)，所以压入返回地址和可能的参数，实际上 Linux 0.12 中 main 会从堆栈获取一些信息，但这里只是示例 */
    pushl $0          /* 参数？可能是环境变量 */
    pushl $0          /* 参数？ */
    pushl $0          /* 参数？ */
    pushl $L6         /* 返回地址（死循环） */
    pushl $main       /* main 函数地址 */
    jmp setup_paging  /* 跳转到分页设置，设置完成后会通过 ret 返回到 main */
L6:
    jmp L6            /* main 返回后死循环 */

ignore_int:     /* 默认中断处理函数，当前仅向屏幕输出字符 'I' 然后返回 */
                /* 保存寄存器 */
    pushl %eax
    pushl %ecx
    pushl %edx
    pushw %ds
    pushw %es
    pushw %fs        
    movl  $0x10, %eax   /* 重新设置数据段为内核数据段 */
    movw  %ax, %ds
    movw  %ax, %es
    movw  %ax, %fs
                        /* 调用 printk */
    movl  $0x96, %edi        /* 显存偏移 0x96（屏幕中间位置） */
    movb  $'I', %al          /* 字符 'I' */
    movb  $0x0c, %ah         /* 属性：红底黑字？0x0c 通常为红色字符 */
    movw  %ax,   %gs:(%edi)  /* 写入显存 */
    popw  %fs           /* 恢复寄存器 */
    popw  %es
    popw  %ds
    popl  %edx
    popl  %ecx
    popl  %eax
    iret                     /* 中断返回 */

.align 4
setup_paging:   /* 设置分页 *//* 首先清空页目录和页表所占的内存区域（从 0 开始，共 5 个页表大小，即 5*4KB=20KB） */
    movl $1024*5, %ecx          /* 循环次数：1024 长字 * 5 = 5120 个长字，即 20KB */
    xorl %eax,     %eax         /* 要写入的值 0 */
    xorl %edi,     %edi         /* 目标地址 0 */
    cld                         /* 清除方向标志，使 stosl 递增 edi */
    rep
    stosl                       /* 将 ecx 个长字写入 edi 指向的内存，清零 */
                    /* 设置页目录项：将页目录的前4个条目分别指向页表 pg0, pg1, pg2, pg3，并加上属性 7（Present, R/W, Supervisor） */
    movl $pg0 + 7, pg_dir           /* 页目录第一项指向 pg0，属性 0x07 */
    movl $pg1 + 7, pg_dir + 4       /* 第二项指向 pg1 */
    movl $pg2 + 7, pg_dir + 8       /* 第三项指向 pg2 */
    movl $pg3 + 7, pg_dir + 12      /* 第四项指向 pg3 */                 
    movl $pg3 + 4092, %edi   /*设置页表项：将物理内存前 4MB 映射到线性地址0~4MB edi 指向 pg3 页表的最后一个页表项（因为我们要从高地址向低地址填充） */
    movl $0xfff007, %eax     /* 第一个页表项：物理地址 0x007（属性 Present, R/W, Supervisor, 且物理地址高20位为 0x000? 这里 0xfff007 实际上对应物理地址 0xfff000? 不对，应该是 0x0000 0007，但这里写 0xfff007 可能是为了映射最后4KB页？实际上典型代码是 movl $0x7, %eax 然后循环加0x1000，但这里直接给了一个很大的数，可能是为了快速填充？需要验证。在 Linux 0.12 源码中，这里是先设置 eax=0x07，然后循环每次加 0x1000。但这里直接用 0xfff007 然后递减？可能是笔误，但我们按原样注释。实际应为从 0x007 开始，每次增加 0x1000，映射 4MB 物理内存。但这里写的是 0xfff007，然后 subl $0x1000, %eax，这样 eax 会变成 0xffe007, 0xffd007 ... 直到负数，这实际上映射的是物理地址 0xfff000, 0xffe000 ... 这是高端内存，而不是低端内存。所以这段代码可能有问题。但原代码如此，我们只能按字面注释。 */
    std                      /* 设置方向标志为递减，使 stosl 递减 edi */
1:
    stosl                    /* 将 eax 写入 [edi]，然后 edi -= 4 */
    subl $0x1000, %eax       /* 页表项指向的物理地址减 4KB */
    jge  1b                  /* 如果 eax >= 0，继续循环 */

    xorl %eax, %eax     /* 分页设置完成，加载页目录地址到 CR3，并开启分页 */
    movl %eax, %cr3          /* 页目录地址为 0（pg_dir 在地址 0） */
    movl %cr0, %eax
    orl  $0x80000000, %eax   /* 设置 CR0 的最高位（PG 位）开启分页 */
    movl %eax, %cr0
    ret                      /* 返回到 after_page_tables 中压入的 main 函数地址 */

.align 4
.word 0
idt_descr:
    .word 256*8-1            /* IDT 界限（256 个中断门，每个 8 字节，减 1） */
    .long idt                /* IDT 基地址 */

.align 4
.word 0
gdt_descr:
    .word 256 * 8 - 1        /* GDT 界限（最多 256 个描述符） */
    .long gdt                /* GDT 基地址 */

.align 8
idt:
    .fill 256, 8, 0          /* IDT 表空间，256 个中断门，每个 8 字节，初始为 0 */

gdt:
    .quad 0x0000000000000000 /* 第一个描述符（NULL 描述符） */
    .quad 0x00c09a0000000fff /* 内核代码段描述符：基址 0，限长 16MB（0xfff 页粒度），代码段，可读，特权级 0 */
    .quad 0x00c0920000000fff /* 内核数据段描述符：基址 0，限长 16MB，数据段，可写，特权级 0 */
    .quad 0x00c0920b8000ffff /* 显存段描述符：基址 0xb8000，限长 64KB，特权级 0？具体看标志 */
    .quad 0x0000000000000000 /* 保留 */
    .fill 251, 8, 0          /* 剩余描述符空间，251 个，每个 8 字节，填 0 */