#include <linux/mm.h>
// 使用内联汇编,获取一个空闲的物理内存页框，并返回其物理地址
unsigned long get_free_page() {
    register unsigned long __res asm("ax") = 0;

repeat:
__asm__("std ; repne ; scasb\n\t"
        "jne 1f\n\t"
        "movb $1,1(%%edi)\n\t"
        "sall $12, %%ecx\n\t"
        "addl %2, %%ecx\n\t"
        "movl %%ecx, %%edx\n\t"
        "movl $1024, %%ecx\n\t"
        "leal 4092(%%edx), %%edi\n\t"
        "xor  %%eax, %%eax\n\t"
        "rep; stosl;\n\t"
        "movl %%edx,%%eax\n"
        "1:"
        :"=a"(__res)
        :""(0), "i"(LOW_MEM), "c"(PAGING_PAGES),
        "D"(mem_map+PAGING_PAGES-1)
        :"dx");

    if (__res >= HIGH_MEMORY)   // 返回地址超过了物理内存最高址，说明没有可用的页框，继续尝试
        goto repeat;
    return __res;
}

