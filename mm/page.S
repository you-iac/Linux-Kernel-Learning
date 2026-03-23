.globl page_fault

page_fault: #实现页中断
    xchgl   %eax, (%esp)
    pushl   %ecx
    pushl   %edx
    pushl   %ds
    pushl   %es
    pushl   %fs
    movl    $0x10, %edx
    movw    %dx, %ds
    movw    %dx, %es
    movw    %dx, %fs
    movl    %cr2, %edx
    pushl   %edx
    pushl   %eax
    testl   $1, %eax
    jne     1f
    jmp     2f
1:  call    do_wp_page  #调用保护函数
2:  addl    $8, %esp    #缺页中断
    popl    %fs
    popl    %es
    popl    %ds
    popl    %edx
    popl    %ecx
    popl    %eax
    iret

