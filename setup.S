/* setup.s 程序
 * 功能：从 bootsect 程序执行完毕后跳转到这里，负责获取硬件参数、移动 system 模块、进入保护模式
 * 运行环境：实模式，代码段地址为 SETUPSEG (0x9020)
 */

/* 常量定义 */
INITSEG = 0x9000   /* 存放硬件参数的内存段地址（也是 bootsect 移动后的位置） */
SYSSEG  = 0x1000   /* system 模块最初加载的段地址（0x10000） */
SETUPSEG = 0x9020  /* setup 程序自身的段地址 */

.code16            /* 16位实模式代码 */
.text
.globl _start_setup

_start_setup:
    /* 设置数据段寄存器 DS 和 ES 为当前代码段 CS（SETUPSEG） */
    movw %cs, %ax
    movw %ax, %ds
    movw %ax, %es

    /* 显示字符串 "setup is running" 到屏幕第3行（DH=3），列0（DL=0） */
    movw $setup_msg, %ax    /* 字符串地址 -> BP */
    movw %ax, %bp
    movw $0x01301, %ax          /* AH=0x13 写字符串，AL=1 光标跟随，BH=0 页号 */
    movw $0x0c, %bx             /* BL=0x0c 字符属性（红底黑字？） */
    movw $(setup_msg_end - setup_msg), %cx   /* 字符串长度 */
    movb $3, %dh                /* 行号 */
    movb $0, %dl                /* 列号 */
    int  $0x010                 /* 调用 BIOS 10H 显示字符串 */

    /* 获取光标位置（并保存到内存 0x90000 处） */
    movw $INITSEG, %ax      /* 切换到 INITSEG 段 */
    movw %ax, %ds
    movb $0x03, %ah         /* AH=0x03 读光标位置 */
    xor  %bh,  %bh          /* 页号 0 */
    int  $0x10              /* 返回：DH=行，DL=列，CX=光标大小（未用） */
    movw %dx, (0)           /* 将光标位置（行、列）存入 [0x90000]（字） */

    /* 获取扩展内存大小（通过 int 0x15, ah=0x88）并保存到 0x90002 */
    movb $0x88, %ah
    int  $0x15              /* 返回：AX=扩展内存大小（KB） */
    movw %ax, (2)           /* 存入 [0x90002] */

    /* 获取显示卡模式信息（通过 int 0x10, ah=0x0f）并保存到 0x90004 和 0x90006 */
    movb $0x0f, %ah         /* AH=0x0f 取当前显示模式 */
    int  $0x10              /* 返回：AH=字符列数，AL=显示模式，BH=当前页号 */
    movw %bx, (4)           /* 存入 [0x90004]：BH=页号，BL 未知？实际存入 BX 整个字 */
    movw %ax, (6)           /* 存入 [0x90006]：AL=模式，AH=列数 */

    /* 获取显示卡配置信息（EGA/VGA 信息）通过 int 0x10, ah=0x12, bl=0x10 */
    movb $0x12, %ah
    movb $0x10, %bl
    int  $0x10              /* 返回：BH=显示内存（0=64k,1=128k,...），BL=显示状态，CH=特性位，CL=切换值 */
    movw %ax, (8)           /* 存入 [0x90008]：AX 可能包含某些信息（？） */
    movw %bx, (10)          /* 存入 [0x9000A]：BX 包含显示内存和状态 */
    movw %cx, (12)          /* 存入 [0x9000C]：CX 包含特性位和切换值 */
    /* 以下固定写入 0x5019 到 0x9000E，可能是硬盘参数表的位置标记？ */
    movw $0x5019, (14)

    /* 获取第一个硬盘的参数表（中断向量 0x41 指向硬盘参数表）并复制到 0x90080 */
    movw $0x0000, %ax       /* 切换到段 0，以便读取中断向量表 */
    movw %ax, %ds
    ldsw (4 * 0x41), %si    /* 取 0x41 号中断向量（4字节），即硬盘1参数表指针 -> DS:SI */
    movw $INITSEG, %ax
    movw %ax, %es           /* 目标段 INITSEG */
    movw $0x0080, %di       /* 目标偏移 0x90080 */
    movw $0x10, %cx         /* 复制 16 字节（硬盘参数表通常 16 字节） */
    rep
    movsb

    /* 获取第二个硬盘的参数表（中断向量 0x46 指向硬盘参数表）并复制到 0x90090 */
    movw $0x0000, %ax
    movw %ax, %ds
    ldsw (4 * 0x46), %si    /* 0x46 号中断向量 -> DS:SI */
    movw $INITSEG, %ax
    movw %ax, %es
    movw $0x0090, %di       /* 目标偏移 0x90090 */
    movw $0x10, %cx
    rep
    movsb

    /* 检测是否存在第二个硬盘，若不存在则清零刚复制的硬盘参数表 */
    movw $0x1500, %ax       /* AH=0x15 读磁盘类型 */
    movb $0x81, %dl         /* 驱动器 0x81（第二个硬盘） */
    int  $0x13
    jc   no_disk1           /* 若出错（CF=1）则无硬盘1？实际上检测的是第二个硬盘，标号可能是 no_disk1 */
    cmpb $3, %ah            /* AH=3 表示存在硬盘（类型代码）？通常 AH=0 表示未安装，AH=3 表示硬盘存在？ */
    je   is_disk1
no_disk1:
    /* 若不存在，则清零 0x90090 处的 16 字节 */
    movw $INITSEG, %ax
    movw %ax, %es
    movw $0x0090, %di
    movw $0x10, %cx
    movw $0x00, %ax
    rep
    stosb                   /* 将 AL（0）写入 ES:DI，重复 CX 次 */

is_disk1:
    /* 准备进入保护模式：关中断 */
    cli

    /* 将 system 模块从 SYSSEG (0x1000) 移动到 0x00000（物理地址 0），为保护模式下的中断向量表和 GDT 等腾出空间 */
    movw $0x0000, %ax       /* 初始源段为 0 */
    cld                     /* 清除方向标志，使 movsw 正向递增 */
do_move:
    movw %ax, %es           /* 目标段 ES = AX（第一次 0，目标地址 0） */
    addw $0x1000, %ax       /* 源段加 0x1000（即每次移动 64KB 后，源段增加 0x1000） */
    cmpw $0x9000, %ax       /* 判断是否移动到 INITSEG（0x9000）？实际 system 模块最大可能到 0x90000？ */
    jz   end_move           /* 如果源段达到 0x9000，说明移动完成（因为 SYSSEG 开始，每次加 0x1000，最多到 0x8000？） */
    movw %ax, %ds           /* 设置源段 DS = AX（当前源段） */
    subw %di, %di           /* 目标偏移 DI = 0 */
    subw %si, %si           /* 源偏移 SI = 0 */
    movw $0x8000, %cx       /* 每次移动 0x8000 个字 = 64KB（因为 0x8000 字 = 0x10000 字节 = 64KB） */
    rep
    movsw                   /* 从 DS:SI 移动到 ES:DI，重复 CX 次 */
    jmp  do_move            /* 继续下一段 */

end_move:
    /* 移动完成，此时 system 模块位于物理地址 0 开始处，占据一定空间，但我们的代码仍运行在 setup 段（0x9020） */
    /* 接下来设置 GS 指向显存段 0xb800，在屏幕上显示一个字符 'A' 作为标记 */
    movw $0xb800, %ax
    movw %ax, %gs
    movb $0xf, %ah          /* 字符属性：白底黑字？0xf 通常为高亮白 */
    movb $0x41, %al         /* 字符 'A' */
    movl $0x100, %edi       /* 屏幕偏移 0x100（第 32 行？实际显存每行 80 字符*2=160 字节，0x100=256 字节，大约在第 2 行第 48 列） */
    movw %ax, %gs:(%di)     /* 写入显存 */

    /* 加载全局描述符表 GDT，gdt_48 指向 GDT 描述符 */
    movw $SETUPSEG, %ax
    movw %ax, %ds
    lgdt gdt_48

    /* 开启 A20 地址线（通过键盘控制器 8042） */
    call empty_8042         /* 等待 8042 输入缓冲空 */
    movb $0xD1, %al         /* 命令：写输出端口 */
    outb %al,  $0x64        /* 写入 8042 命令端口 */
    call empty_8042
    movb $0xDF, %al         /* 数据：置 A20 线为高（开启） */
    outb %al,  $0x60        /* 写入 8042 数据端口 */
    call empty_8042

    /* 重新编程 8259 中断控制器，将中断向量号重新映射 */
    movb $0x11, %al         /* ICW1: 边沿触发，级联，需要 ICW4 */
    outb %al,  $0x20        /* 主片 8259A 端口 0x20 */
    .word 0x00eb, 0x00eb    /* 两个短跳转作为延迟（相当于 jmp .+2） */
    outb %al,  $0xA0        /* 从片 8259A 端口 0xA0 */
    .word 0x00eb, 0x00eb
    movb $0x20, %al         /* ICW2: 主片中断向量起始号 0x20 */
    outb %al, $0x21         /* 主片数据端口 0x21 */
    .word 0x00eb, 0x00eb
    movb $0x28, %al         /* ICW2: 从片中断向量起始号 0x28 */
    outb %al,   $0xA1       /* 从片数据端口 0xA1 */
    .word 0x00eb, 0x00eb
    movb $0x04, %al         /* ICW3: 主片 IR2 连接从片 */
    outb %al, $0x21
    .word 0x00eb, 0x00eb
    movb $0x02, %al         /* ICW3: 从片连接主片 IR2 */
    outb %al, $0xA1
    .word 0x00eb, 0x00eb
    movb $0x01, %al         /* ICW4: 8086 模式，正常 EOI，非缓冲 */
    outb %al, $0x21
    .word 0x00eb, 0x00eb
    outb %al, $0xA1
    .word 0x00eb, 0x00eb
    /* 屏蔽所有中断（除级联外） */
    movb $0xff, %al         /* 屏蔽所有 IRQ */
    outb %al, $0x21
    .word 0x00eb, 0x00eb
    outb %al, $0xA1

    /* 开启保护模式（设置 CR0 的 PE 位） */
    movl %cr0, %eax
    xorb $1, %al            /* 将最低位（PE 位）置 1 */
    movl %eax, %cr0

    /* 远跳转进入保护模式，跳转到 0x0000:0 ？实际上应该是跳转到 32 位代码的入口，即 system 模块的 head 部分 */
    .byte 0x66, 0xea        /* 操作数大小前缀和远跳转操作码，形成 32 位偏移的远跳转 */
    .long 0x0               /* 32 位偏移地址（EIP = 0） */
    .word 0x8               /* 段选择子（0x8 是 GDT 的第二项，即内核代码段） */

/* 子程序：等待 8042 键盘控制器输入缓冲为空 */
empty_8042:
    .word 0x00eb, 0x00eb    /* 延迟 */
    inb $0x64, %al          /* 读状态寄存器 */
    testb $2, %al           /* 测试位 1（输入缓冲满标志） */
    jnz empty_8042          /* 若满则继续等待 */
    ret

/* 全局描述符表 GDT */
gdt:
    .word 0,0,0,0           /* 第一个描述符（NULL 描述符） */

    /* 第二个描述符：内核代码段，基址 0，限长 0x7ff（8MB？因为粒度位设为页？实际 GDT 条目中限长为 20 位，加上粒度位 G=1 则乘以 4KB，所以 0x7ff * 4KB = 8MB - 4KB？）这里限长 0x07ff，G=1（在段属性高字节 0xC0 中设置了 G=1），因此段限长 8MB-4KB */
    .word 0x07ff            /* 段限长低 16 位 */
    .word 0x0000            /* 基址低 16 位 */
    .word 0x9A00            /* 基址高 8 位（0） + 段属性：0x9A = 存在、特权级0、代码段、可读、非一致 */
    .word 0x00C0            /* 基址最高 8 位（0） + 段属性高 4 位：0xC = 粒度 4KB、32 位模式 */

    /* 第三个描述符：内核数据段，基址 0，限长 0x07ff，属性 0x92 = 存在、数据段、可写 */
    .word 0x07ff
    .word 0x0000
    .word 0x9200
    .word 0x00c0

    /* 第四个描述符：显存段，基址 0xb8000，限长 0xffff（64KB），属性 0x920b? 实际上 0x920b 的低位 0x0b 表示段类型？标准 Linux 0.12 中此处为 0x920b，但属性可能是 0x92（数据段）加上基址高 8 位？需仔细分析：通常显存段描述符为 0x00c0920b8000ffff，即基址 0xb8000，限长 64KB，数据段，特权级0，32位 */
    .word 0xffff
    .word 0x8000            /* 基址低 16 位 = 0x8000，与高 8 位组合得到 0xb8000？基址实际是 0x0b8000，这里高 8 位在 .word 0x00c0 中？不对，这个描述符由四个字组成：低16位限长，低16位基址，高16位基址+属性，最高16位基址+属性。所以显存段描述符：
       第一个 .word 0xffff 限长低16位
       第二个 .word 0x8000 基址低16位 (0x8000)
       第三个 .word 0x920b 基址中8位 (0x0b) + 属性 (0x92) 合并为 0x920b，所以基址中8位是 0x0b
       第四个 .word 0x00c0 基址高8位 (0x00) + 属性高 (0xc0)
       因此基址 = 0x00 0x0b 0x8000 = 0x00b8000，正确 */
    .word 0x920b
    .word 0x00c0

/* GDT 描述符：用于 lgdt 指令 */
gdt_48:
    .word 0x800             /* GDT 限长 = 0x800 字节，即 256 个描述符（每个 8 字节） */
    .word 512+gdt, 0x9      /* 线性基地址 = 0x90000 + gdt 的偏移？因为当前 DS=SETUPSEG (0x9020)，gdt 在当前段内，所以物理地址 = (SETUPSEG<<4) + offset(gdt) = 0x90200 + offset。这里直接写 512+gdt，因为 gdt 在 setup 段内的偏移？实际上 512+gdt 表示从 0x90200 加上 512（0x200）再加上 gdt 偏移，结果应该是 gdt 的物理地址。而 0x9 可能是段选择子？但此处是描述符的高 16 位？不对，gdt_48 应该是一个 6 字节的结构：2 字节限长 + 4 字节基址。这里 .word 0x800 是限长，后面 .word 512+gdt, 0x9 组成基址（低16位和高16位）。基址 = (0x9 << 16) + (512+gdt) = 0x90000 + (512+gdt)。由于 gdt 在当前段偏移量小于 512，所以基址在 0x90200 附近。正确，这样 GDT 的线性地址就在 0x90000 之后，而 system 模块在 0 开始，不会冲突。 */
    .word 512+gdt, 0x9

/* 显示字符串数据 */
setup_msg:
  .ascii "setup is running..."
setup_msg_end: