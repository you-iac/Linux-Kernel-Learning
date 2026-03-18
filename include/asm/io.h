#ifndef _IO_H
#define _IO_H
/*@brief 向指定端口发送一个字节 (Output Byte) @param value 要发送的 8 位数据 (放入 AL 寄存器) @param port  目标 I/O 端口地址 (放入 DX 寄存器)*/ 
#define outb(value,port) \
__asm__ ("outb %%al,%%dx"::"a" (value),"d" (port))
/* @brief 从指定端口读取一个字节 (Input Byte) @param port I/O 端口地址 (放入 DX 寄存器) @return 读取到的 8 位数据 (从 AL 寄存器返回)*/
#define inb(port) ({ \
    unsigned char _v; \
    __asm__ volatile ("inb %%dx,%%al":"=a" (_v):"d" (port)); \
    _v; \
})
/* @brief 向端口发送字节并等待 通过连续跳转实现短暂延时。*/
#define outb_p(value, port)     \
__asm__("outb %%al, %%dx\n"     \
        "\tjmp 1f\n"            \
        "1:\tjmp 1f\n"          \
        "1:" :: "a"(value), "d"(port))
/** @brief 从端口读取字节并等待 通过连续跳转实现短暂延时。*/
#define inb_p(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al\n" \
    "\tjmp 1f\n" \                             
    "1:\tjmp 1f\n" \
    "1:":"=a" (_v):"d" (port)); \
_v; \
})

#endif

