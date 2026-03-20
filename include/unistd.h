#ifndef _UNISTD_H
#define _UNISTD_H

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifdef __LIBRARY__

#define __NR_fork 0 // fork系统调用的编号
/* 系统调用0的实体宏实现，type是返回类型，name是函数名*//*利用NR+name的方式动态选择调用的编号*/
#define _syscall0(type, name)   \   
type name() {                   \
    long __res;                 \
__asm__ volatile("int $0x80\n\r" \
        : "=a"(__res)            \
        : "a"(__NR_##name));    \ 
    if (__res >= 0)             \
        return (type)__res;     \
    errno = -__res;             \
    return -1;                  \
}

#endif /* __LIBRARY__ */

extern int errno;

static int fork();

#endif

