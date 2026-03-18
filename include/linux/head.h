#ifndef _HEAD_H //Linux 内核的头文件，包含了内核开发中常用的基础定义、数据结构和函数声明等
#define _HEAD_H

 
typedef struct desc_struct {    // 描述符结构体，包含了段界限、基地址和访问权限等信息
    unsigned long a, b;
} desc_table[256];  // 描述符表，包含了256个描述符项

extern unsigned long pg_dir[1024];  // 页目录表，包含了1024个页目录项，
extern desc_table idt,gdt;          // 中断描述符表（IDT）和全局描述符表（GDT），分别包含了256个描述符项，用于定义中断处理程序和内存段的属性

#endif

