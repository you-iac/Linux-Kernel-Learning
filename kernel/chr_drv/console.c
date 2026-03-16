#include <linux/tty.h>          /* 终端相关定义 */

/*
 * 以下宏用于读取实模式下由 BIOS 或引导程序存放在物理内存 0x90000 区域
 * 的显示参数。这些参数在系统启动时由 setup.s 从 BIOS 获取并保存。
 * 地址均为物理地址，因为数据段为从0开始，虚拟地址直接是物理地址
 * 
 * 根据显存信息，使用指针指向显存地址，向右上角依次字符，从而实现信息打印
 */
#define ORIG_VIDEO_PAGE     (*(unsigned short *)0x90004) /* 当前显示页 */
#define ORIG_VIDEO_MODE     ((*(unsigned short *)0x90006) & 0xff) /* 显示模式（低字节） */
#define ORIG_VIDEO_COLS     (((*(unsigned short *)0x90006) & 0xff00) >> 8) /* 屏幕列数（高字节） */
#define ORIG_VIDEO_LINES    ((*(unsigned short *)0x9000e) & 0xff) /* 屏幕行数 */
#define ORIG_VIDEO_EGA_AX   (*(unsigned short *)0x90008) /* EGA 寄存器 AX 值 */
#define ORIG_VIDEO_EGA_BX   (*(unsigned short *)0x9000a) /* EGA 寄存器 BX 值（含显示类型信息） */
#define ORIG_VIDEO_EGA_CX   (*(unsigned short *)0x9000c) /* EGA 寄存器 CX 值 */

/* 显示卡类型常量 */
#define VIDEO_TYPE_MDA      0x10    /* 单色文本显示器（MDA） */
#define VIDEO_TYPE_CGA      0x11    /* CGA 显示器 */
#define VIDEO_TYPE_EGAM     0x20    /* EGA/VGA 工作在单色模式 */
#define VIDEO_TYPE_EGAC     0x21    /* EGA/VGA 工作在彩色模式 */

/* 全局静态变量，保存当前显示参数 */
static unsigned char    video_type;     /* 显示卡类型 */
static unsigned long    video_num_columns;  /* 屏幕列数 */
static unsigned long    video_num_lines;    /* 屏幕行数（原注释误写为 test） */
static unsigned long    video_mem_base;     /* 视频内存起始物理地址 */
static unsigned long    video_mem_term;     /* 视频内存结束物理地址（用于边界检查） */
static unsigned long    video_size_row;     /* 每行占用的字节数（列数 * 2，每个字符占2字节） */
static unsigned char    video_page;     /* 当前显示页 */
static unsigned short   video_port_reg;     /* 显卡寄存器选择端口（索引端口） */
static unsigned short   video_port_val;     /* 显卡寄存器数据端口（值端口） */

/**
 * con_init - 控制台初始化函数
 * 根据引导时保存的显示参数，检测显示卡类型并设置相关变量，
 * 同时在屏幕右上角显示显示卡类型标识（用于调试/提示）。
 */
void con_init() {
    char * display_desc = "????";    /* 默认显示描述字符串 */
    char * display_ptr;              /* 用于写入显存的指针 */

    /* 从 BIOS 参数区域获取基本显示信息 */
    video_num_columns = ORIG_VIDEO_COLS;          /* 列数 */
    video_size_row = video_num_columns * 2;       /* 每行字节数（字符+属性） */
    video_num_lines = ORIG_VIDEO_LINES;           /* 行数 */
    video_page = ORIG_VIDEO_PAGE;                  /* 当前页 */

    /* 判断是否为单色显示器（MDA 模式码为 7） */
    if (ORIG_VIDEO_MODE == 7) {
        /* 单色显示器：显存基址 0xB0000，端口 0x3B4/0x3B5 */
        video_mem_base = 0xb0000;
        video_port_reg = 0x3b4;
        video_port_val = 0x3b5;
        /* ORIG_VIDEO_EGA_BX 的低字节若不为 0x10，说明是 EGA/VGA 单色模式 */
        if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10) {
            video_type = VIDEO_TYPE_EGAM;          /* EGA 单色模式 */
            video_mem_term = 0xb8000;               /* 显存结束地址（约32KB） */
            display_desc = "EGAm";                  /* 显示 "EGAm" */
        } else {
            video_type = VIDEO_TYPE_MDA;            /* 标准 MDA */
            video_mem_term = 0xb2000;                /* 显存结束地址（MDA 通常只有 4KB） */
            display_desc = "*MDA";                   /* 显示 "*MDA" */
        }
    } else { /* 彩色显示器（包括 CGA、EGA 彩色等） */
        /* 彩色显示器：显存基址 0xB8000，端口 0x3D4/0x3D5 */
        video_mem_base = 0xb8000;
        video_port_reg  = 0x3d4;
        video_port_val  = 0x3d5;

        /* 同样通过 EGA_BX 低字节判断是否为真正的 EGA/VGA 彩色模式 */
        if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10) {
            video_type = VIDEO_TYPE_EGAC;            /* EGA/VGA 彩色模式 */
            video_mem_term = 0xc0000;                 /* 显存结束地址（最大 128KB，但实际只用到部分） */
            display_desc = "EGAc";                    /* 显示 "EGAc" */
        } else {
            video_type = VIDEO_TYPE_CGA;              /* 标准 CGA */
            video_mem_term = 0xba000;                  /* 显存结束地址（CGA 通常 16KB） */
            display_desc = "*CGA";                     /* 显示 "*CGA" */
        }
    }

    /*
     * 在屏幕右上角显示显示卡类型标识。
     * 计算指针：显存基址 + (行数-1)*每行字节数 + (列数-4) ，
     * 即最后一行的倒数第 4 个字符位置（因为标识通常占4个字符）。
     * 每个字符在显存中占2字节（低字节为字符，高字节为属性），
     * 这里只写字符，属性使用原有值（通过 *display_ptr++ 跳过属性字节）。
     */
    display_ptr = ((char *)video_mem_base) + video_size_row - 8;
    while (*display_desc) {
        *display_ptr++ = *display_desc++;   /* 写入字符 */
        *display_ptr++;                      /* 跳过属性字节 */
    }
}