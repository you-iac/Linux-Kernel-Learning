.code32
.text
.globl system_call, timer_interrupt, sys_fork

EAX        = 0x00
EBX        = 0x04
ECX        = 0x08
EDX        = 0x0C
ORIG_EAX   = 0x10
FS         = 0x14
ES         = 0x18
DS         = 0x1c
EIP        = 0x20
CS         = 0x24
EFLAGS     = 0x28
OLDESP     = 0x2c
OLDSS      = 0x30

int_msg:
    .asciz  "In kernel interrupt\n\r"

system_call:        # 0x80 系统调用入口（由中断向量 0x80 跳转）
    pushl %ds       # 保存用户态 DS
    pushl %es       # 保存用户态 ES
    pushl %fs       # 保存用户态 FS
    pushl %eax      # 保存 EAX（系统调用号 / 将被用作表索引）
    pushl %edx      # 保存 EDX
    pushl %ecx      # 保存 ECX
    pushl %ebx      # 保存 EBX
    movl  $0x10, %edx
    movw  %dx, %ds  # 切换 DS 到内核数据段 (选择子 0x10)
    movw  %dx, %es  # 切换 ES 到内核数据段
    movl  $0x17, %edx
    movw  %dx, %fs  # 切换 FS 到内核内置段 (例如线程数据区)

    call sys_call_table(, %eax, 4)  # 根据 %eax（调用号）间接调用处理函数
    pushl %eax      # 将系统调用返回值压栈，ret_from_sys_call 将用 pop 恢复

ret_from_sys_call:
    popl %eax       # 恢复 EAX
    popl %ebx       # 恢复 EBX
    popl %ecx       # 恢复 ECX
    popl %edx       # 恢复 EDX
    addl $4, %esp   # 弹出之前为返回值占位的 4 字节
    popl %fs        # 恢复用户态 FS
    popl %es        # 恢复用户态 ES
    popl %ds        # 恢复用户态 DS
    iret            # 使用 IRET 返回用户态，恢复 eip/eflags/ss/esp

.align 4
timer_interrupt:        # 时间中断（IRQ0）入口
    pushl %ds       # 保存 DS
    pushl %es       # 保存 ES
    pushl %fs       # 保存 FS
    pushl $-1       # 占位 ORIG_EAX (-1 表示非系统调用上下文)
    pushl %edx      # 保存 EDX
    pushl %ecx      # 保存 ECX
    pushl %ebx      # 保存 EBX
    pushl %eax      # 保存 EAX
    movl  $0x10, %eax
    movw  %ax, %ds  # 切换 DS 到内核数据段
    movw  %ax, %es  # 切换 ES 到内核数据段
    movl  $0x17, %eax
    movw  %ax, %fs  # 切换 FS 到内核段
    movb  $0x20, %al
    outb  %al, $0x20  # 向 PIC 发送 EOI，通知中断处理完成
    movl  CS(%esp), %eax
    andl  $3, %eax   # 计算 CPL = CS & 3，判断中断时是否在用户态
    pushl %eax       # 将 CPL 作为参数传给 do_timer
    call  do_timer   # 调用时钟/调度处理函数
    addl  $4, %esp
    jmp   ret_from_sys_call

.align 4
sys_fork:   # 创建新进程（fork 系统调用实现入口）
    call  find_empty_process    # 查找空闲进程表项，返回在 %eax（<0 则失败）
    testl %eax, %eax           # 测试返回值是否为负（失败标志）
    js    1f                   # 若负则跳转到标签 1，直接返回错误
    pushl %gs                  # 保存段寄存器 GS
    pushl %esi                 # 保存 ESI
    pushl %edi                 # 保存 EDI
    pushl %ebp                 # 保存 EBP
    pushl %eax                 # 将新进程号或指针压栈，作为参数传给 copy_process
    call  copy_process         # 复制进程（创建子进程），返回值放 %eax
    addl  $20, %esp            # 清理上面 push 的 5 个参数（5*4=20 字节）
1:  ret                         # 返回：父进程返回子 pid，子进程返回 0 或按 copy_process 设定