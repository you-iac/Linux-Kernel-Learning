/* 系统模块大小（字节数？实际上是节数？0x3000 = 12288，但通常指系统模块占用的内存大小，单位是节（16字节）？这里保持原样注释） */
SYSSIZE = 0x3000

/* setup 模块占用的扇区数（这里为4个扇区） */
SETUPLEN  = 4

/* bootsect 代码被加载到的段地址（0x7c0，即物理地址0x7c00处） */
BOOTSEG   = 0x7c0

/* 将要移动到的位置（0x9000段，即物理地址0x90000） */
INITSEG   = 0x9000

/* setup 模块将被加载到的段地址（0x9020，即物理地址0x90200） */
SETUPSEG  = 0x9020

/* system 模块将被加载到的段地址（0x1000，即物理地址0x10000） */
SYSSEG    = 0x1000

/* system 模块结束的段地址（SYSSEG + SYSSIZE，但SYSSIZE是字节数还是节数？此处SYSSIZE=0x3000，若为节（16字节）则结束段为0x4000，但通常SYSSIZE以字节为单位，则结束段为SYSSEG + (SYSSIZE >> 4) = 0x1000 + 0x300 = 0x1300，但此处是段地址，故应为SYSSEG + (SYSSIZE>>4)）但代码中直接用作段比较，所以SYSSIZE应为节数？我们按代码逻辑注释，不深究 */
ENDSEG    = SYSSEG + SYSSIZE

/* 根文件系统设备号，0x000表示当前软盘 */
ROOT_DEV  = 0x000

/* 以下是16位实模式代码 */
.code16
.text

.global _start
_start:

  /* 长跳转到BOOTSEG:start2处，这里BOOTSEG=0x7c0，段内偏移start2（下一条指令），由于当前就在0x7c0段，所以此跳转主要是为了确保CS正确 */
  jmpl $BOOTSEG, $start2

start2:
  /* 设置DS为BOOTSEG（0x7c0），以便访问本段内的数据 */
  movw $BOOTSEG, %ax
  movw %ax, %ds
  /* 设置ES为INITSEG（0x9000），目标段 */
  movw $INITSEG, %ax
  movw %ax, %es
  /* 设置计数器CX=256，即移动256个字（512字节），因为一个扇区512字节，刚好将bootsect自身移动到0x90000处 */
  movw $256, %cx
  /* SI=0，DI=0，即从本段偏移0复制到ES段偏移0 */
  subw %si, %si
  subw %di, %di

  /* 重复执行movsw，将DS:SI内容复制到ES:DI，共CX次，每次移动一个字 */
  rep
  movsw

  /* 长跳转到INITSEG:go处，即移动到新位置后的go标号处继续执行 */
  jmpl $INITSEG, $go

go:
  /* 设置各段寄存器为当前CS（即INITSEG） */
  movw %cs, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %ss
  /* 设置栈顶指针SP=0xFF00（栈向下生长） */
  movw $0xFF00, %sp

/* 加载setup模块（从磁盘读取SETUPLEN个扇区到SETUPSEG段） */
load_setup:
  /* 驱动器号DL=0（软盘A），磁头号DH=0，磁道号CH=0，扇区号CL=2（从第2扇区开始） */
  movw $0x0000, %dx
  movw $0x0002, %cx
  /* ES:BX指向缓冲区，此处ES为INITSEG，BX=0x0200（即INITSEG:0x0200，物理地址0x90200，也就是SETUPSEG段开始） */
  movw $0x0200, %bx
  /* AL = SETUPLEN，即要读取的扇区数 */
  movb $SETUPLEN, %al
  /* AH = 0x02，表示读扇区功能 */
  movb $0x02,   %ah
  /* 调用BIOS 0x13中断读磁盘 */
  int  $0x13
  /* 若成功（CF=0），跳转到ok_load_setup */
  jnc  ok_load_setup
  /* 若读失败，则复位磁盘系统（AH=0，DL=驱动器号） */
  movw $0x0000, %dx
  movw $0x0000, %ax
  int  $0x13
  /* 重试加载setup */
  jmp  load_setup

ok_load_setup:
  /* 获取磁盘参数：读取软盘每磁道扇区数 */
  movb $0x00, %dl     /* 驱动器号0 */
  movw $0x0800, %ax   /* AH=0x08 读驱动器参数 */
  int  $0x13
  /* 返回参数：CL的低5位为每磁道最大扇区数（1-63），存入sectors变量 */
  movb $0x00, %ch     /* 只保留CL部分 */
  movw %cx, %cs:sectors

  /* 设置ES为INITSEG，为后续调用read_it准备 */
  movw $INITSEG, %ax
  movw %ax, %es

  /* 设置ES为SYSSEG，准备读取system模块到0x10000处 */
  movw $SYSSEG, %ax
  movw %ax, %es
  /* 调用read_it读取system模块 */
  call read_it
  /* 关闭软驱电机 */
  call kill_motor

  /* 检查根设备是否已定义（root_dev变量） */
  movw %cs:root_dev, %ax
  cmpw $0, %ax
  jne  root_defined
  /* 如果未定义，则根据每磁道扇区数确定根设备号：如果每磁道15扇区（1.2M软盘），则根设备为0x0208（可能是/dev/PS0？）如果18扇区（1.44M），则为0x021c */
  movw %cs:sectors, %bx
  movw $0x0208, %ax   /* 1.2M软盘根设备号 */
  cmpw $15, %bx
  je   root_defined
  movw $0x021c, %ax   /* 1.44M软盘根设备号 */
  cmpw $18, %bx
  je   root_defined
  /* 如果都不是，则死循环（无法确定根设备） */
undef_root:
  jmp  undef_root
root_defined:
  /* 保存确定的根设备号到root_dev变量 */
  mov  %ax, %cs:root_dev

  /* 重新设置数据段为当前CS（INITSEG） */
  movw %cs, %ax
  movw %ax, %ds
  movw %ax, %es

  /* 清除屏幕（AH=0x06 初始化窗口，AL=0 清全屏，BH=0x07 黑底白字，CH/CL/DH/DL窗口范围） */
  movw $0x600, %ax
  movw $0x700, %bx
  movw $0x0,   %cx
  movw $0x184f,%dx
  int  $0x10

  /* 显示启动信息 "Linux is booting..." */
  movw $msg, %ax      /* 字符串偏移地址 */
  movw %ax, %bp       /* ES:BP指向字符串 */
  movw $0x01301, %ax  /* AH=0x13 写字符串，AL=1 光标移动，BH=0 页号，BL=0x0c 字符属性（红底？） */
  movw $0x0c, %bx
  movw $23, %cx       /* 字符串长度 */
  movw $0, %dx        /* 起始行列（0,0） */
  int  $0x010

  /* 最后跳转到SETUPSEG:0处，开始执行setup程序 */
  jmpl $SETUPSEG, $0

/* 以下是read_it子程序，用于从磁盘读取system模块到内存 */
/* sread: 当前磁道已读扇区数，初始值为1+SETUPLEN（即已经读过了bootsect和setup） */
sread:
  .word 1+SETUPLEN
/* head: 当前磁头号 */
head:
  .word 0
/* track: 当前磁道号 */
track:
  .word 0

read_it:
  /* 检查ES段地址是否以0x1000对齐（即段地址低12位为0），否则死机 */
  movw %es, %ax
  testw $0x0fff, %ax
die:
  jne die
  /* 清零BX，BX为段内偏移 */
  xor %bx, %bx
rp_read:
  /* 检查是否已经读取到ENDSEG段，如果ES >= ENDSEG，则读取完成，返回 */
  movw %es, %ax
  cmpw $ENDSEG, %ax
  jb   ok1_read
  ret
ok1_read:
  /* 计算当前磁道剩余扇区数 = sectors - sread */
  movw %cs:sectors, %ax
  subw sread, %ax
  movw %ax, %cx
  /* 将剩余扇区数转换为字节数（左移9位，即乘以512） */
  shlw $9,  %cx
  /* 加上当前段内偏移BX，检查是否跨段（超过64KB） */
  addw %bx, %cx
  jnc  ok2_read    /* 若无进位，则不会跨段，跳转 */
  je   ok2_read    /* 若刚好等于0，说明正好到段边界，也跳转 */
  /* 若跨段，则计算能读的最大扇区数，使读完后不跨段 */
  xorw %ax, %ax
  subw %bx, %ax    /* ax = 0 - bx，即本段剩余字节数 */
  shrw $9,  %ax    /* 除以512得到扇区数 */
ok2_read:
  /* 调用read_track读磁道，参数ax为要读的扇区数 */
  call read_track
  /* 更新sread：加上本次读取的扇区数ax */
  movw %ax, %cx
  addw sread, %ax
  /* 如果sread+本次读扇区数等于每磁道扇区数，说明本磁道已读完，需要切换磁头或磁道 */
  cmpw %cs:sectors, %ax
  jne  ok3_read
  /* 当前磁道读完，准备下一磁头或磁道 */
  movw $1, %ax
  subw head, %ax    /* 若head为0则ax=1，若head为1则ax=0 */
  jne  ok4_read
  /* 若head为1（ax非0，即ne条件满足），则切换到下一磁道 */
  incw track
ok4_read:
  /* 设置head为新值（若原head=0则变为1，若原head=1则变为0） */
  movw %ax, head
  /* 重置sread为0，因为新磁道从头开始读 */
  xorw %ax, %ax
ok3_read:
  /* 更新sread为当前磁道已读扇区数 */
  movw %ax, sread
  /* 将本次读取的扇区数转换为字节数，加到bx上 */
  shlw $9,  %cx
  addw %cx, %bx
  /* 如果bx未进位（即未超出64KB），则继续读本段 */
  jnc  rp_read
  /* 若进位，说明本段已满（bx超过65535），则切换到下一个段（ES加0x1000，即64KB），bx清零 */
  movw %es, %ax
  addw $0x1000, %ax
  movw %ax, %es
  xorw %bx, %bx
  jmp  rp_read

/* 读磁道子程序：读取当前磁道（track, head）从sread+1开始的ax个扇区 */
read_track:
  pushw %ax
  pushw %bx
  pushw %cx
  pushw %dx
  /* 设置磁道号（CH），扇区起始号（CL） */
  movw  track, %dx
  movw  sread, %cx
  incw  %cx          /* 因为sread记录的是已读扇区数，下一个要读的扇区号 = sread+1 */
  movb  %dl, %ch     /* 磁道号放入CH */
  movw  head, %dx
  movb  %dl, %dh     /* 磁头号放入DH */
  movb  $0, %dl      /* 驱动器号0 */
  andw  $0x0100, %dx /* 这个操作没有意义？可能原意是只保留磁头标志？实际不影响，可忽略 */
  movb  $2, %ah      /* 读扇区功能号 */
  int   $0x13
  jc    bad_rt       /* 若读失败，跳转错误处理 */
  /* 成功则恢复寄存器并返回 */
  popw  %dx
  popw  %cx
  popw  %bx
  popw  %ax
  ret
bad_rt:
  /* 读失败，复位磁盘重试 */
  movw $0x0, %ax     /* AH=0 复位磁盘 */
  movw $0x0, %dx     /* 驱动器0 */
  int  $0x13
  popw %dx
  popw %cx
  popw %bx
  popw %ax
  jmp read_track

/* 关闭软驱电机：向端口0x3f2写0，关闭电机和驱动器选择 */
kill_motor:
  pushw %dx
  movw  $0x3f2, %dx
  movb  $0,  %al
  outb  %al, %dx
  popw  %dx
  ret

/* 存放每磁道扇区数的变量 */
sectors:
  .word 0

/* 启动信息字符串，前有回车换行，后有回车换行 */
msg:
.byte 13, 10
.ascii "Linux is booting..."
.byte 13, 10

/* 将root_dev定位到偏移508处（512字节扇区最后4字节的前2字节） */
.org 508
root_dev:
  .word ROOT_DEV

/* 最后两个字节是启动标志0xaa55，表示这是可启动扇区 */
boot_flag:
  .word 0xaa55