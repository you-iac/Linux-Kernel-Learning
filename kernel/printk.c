#include <stdarg.h>
#include <stddef.h>
#include <linux/kernel.h>

static char buf[1024];

extern int vsprintf(char* buf, const char* fmt, va_list args);
/// @brief 模仿 C 语言的 printf @param fmt 格式化字符串 @param  @return 打印字符数
int printk(const char* fmt, ...) {  /*调用vsprintf处理buf，使用tty_write输入终端*/
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);

    __asm__("pushw %%fs\n\t"
            "pushw %%ds\n\t"
            "popw  %%fs\n\t"
            "pushl %0\n\t"
            "pushl $buf\n\t"    // 这里的 $buf 是我们要打印的字符串
            "pushl $0\n\t"          
            "call  tty_write\n\t"   // 调用 TTY 层的写入函数
            "addl  $8, %%esp\n\t"
            "popl  %0\n\t"
            "popw  %%fs"
            ::"r"(i):"ax", "cx", "dx");

    return i;
}

