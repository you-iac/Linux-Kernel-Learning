#ifndef _SYSTEM_H
#define _SYSTEM_H

#define sti()   __asm__("sti"::)    //启用中断
#define cli()   __asm__("cli"::)    //禁用中断
#define nop()   __asm__("nop"::)    //执行空操作
#define iret()  __asm__("iret"::)   //从中断返回

#endif

