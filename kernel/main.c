#define __LIBRARY__

#include <linux/tty.h>
#include <linux/kernel.h>

void main(void) {
    tty_init();
    char* s = "world";
    
    printk("hello %s!/n", s);
    __asm__ __volatile__(
            "int $0x80\n\r"
            "loop:\n\r"
            "jmp loop"
            ::);
}
