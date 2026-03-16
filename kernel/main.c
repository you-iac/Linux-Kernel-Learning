#define __LIBRARY__

#include <linux/tty.h>
/// @brief 死循环陷阱
/// @param  
void main(void) 
{
    tty_init();
    __asm__("int $0x80 \n\r"::);
    __asm__ __volatile__(
            "loop:\n\r"
            "jmp loop"
            ::);
}
