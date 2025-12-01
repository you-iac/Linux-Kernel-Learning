BOOTSEG = 0x7c0
.code16 
.text
.global _start          #设置全局代码入口

_start:                 #代码入口
    jmpl $BOOTSEG, $start2  

start2 :                #设置相关寄存器，准备调用中断
    movw $BOOTSEG, %ax 
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    movw $msg, %ax
    movw %ax, %bp
    movw $0x01301, %ax
    movw $0x0c, %bx # bh = 0，bl = 0c(红色)
    movw $12, %cx # cx = 串长度
    movb $0, %dl
    int $0x010 # 10h号中断,通知显卡刷新内容

loop:           #中断结束，死循环，防止CPU出现不可控情况
    jmp loop

msg:
.ascii "Hello World!"

.org 510
boot_flag:
    .word 0xaa55

