#include <linux/sched.h>    /* 包含进程调度相关的定义，如 PAGING_PAGES、MAP_NR、USED 等宏 */

unsigned long HIGH_MEMORY = 0;/* 记录物理内存的最高地址（字节），将在 mem_init 中初始化为 end_mem */

unsigned char mem_map [ PAGING_PAGES ] = {0,};/* mem_map 是物理内存页框的使用状态数组 每个元素对应一个物理页*/
// 释放一个物理页框，参数 addr 是页框的物理地址
void free_page(unsigned long addr) {
    if (addr < LOW_MEM) return;
    if (addr >= HIGH_MEMORY)
        printk("trying to free nonexistent page");

    addr -= LOW_MEM;    
    addr >>= 12;        /* 将物理地址转换为页框号（物理地址右移12位） */
    if (mem_map[addr]--) return;
    mem_map[addr]=0;
    printk("trying to free free page");
}
// 释放指定地址范围内的页表，参数 from 是起始地址，size 是要释放的字节数
int free_page_tables(unsigned long from,unsigned long size) {
    unsigned long *pg_table;               // 页表项指针
    unsigned long * dir, nr;               // 页目录指针与计数器
    
    if (from & 0x3fffff)                   // 要求按 4MB 对齐（22 位）
        printk("free_page_tables called with wrong alignment");
    if (!from)                             // 不能释放内核的起始表
        printk("Trying to free up swapper memory space");
    size = (size + 0x3fffff) >> 22;        // 计算需要处理的页目录项数
    dir = (unsigned long *) ((from>>20) & 0xffc); // 计算起始页目录表项地址

    for ( ; size-->0 ; dir++) {            // 遍历每个页目录项
        if (!(1 & *dir))                   // 如果目录项无效则跳过
            continue;
        pg_table = (unsigned long *) (0xfffff000 & *dir); // 取页表基址
        for (nr=0 ; nr<1024 ; nr++) {     // 遍历页表的 1024 个页表项
            if (*pg_table) {
                if (1 & *pg_table)
                    free_page(0xfffff000 & *pg_table); // 若页项存在且可释放则释放物理页
                *pg_table = 0;              // 清空页表项
            }
            pg_table++;
        }
        free_page(0xfffff000 & *dir);      // 释放页表本身占用的物理页
        *dir = 0;                          // 清空页目录项
    }
    invalidate();                          // 刷新 TLB
    return 0;                              // 成功
}
// 复制页表，参数 from 是源地址，to 是目标地址，size 是要复制的字节数
int copy_page_tables(unsigned long from,unsigned long to,long size) {
    unsigned long * from_page_table;
    unsigned long * to_page_table;
    unsigned long this_page;
    unsigned long * from_dir, * to_dir;
    unsigned long nr;

    if ((from&0x3fffff) || (to&0x3fffff)) { // 校验 4MB 对齐
        printk("copy_page_tables called with wrong alignment");
    }

    /* Get high 10 bits. As PDE is 4 byts, so right shift 20.*/
    from_dir = (unsigned long *) ((from>>20) & 0xffc); // 源页目录地址
    to_dir = (unsigned long *) ((to>>20) & 0xffc);     // 目标页目录地址

    size = ((unsigned) (size+0x3fffff)) >> 22;        // 需要复制的页目录项数
    for( ; size-->0 ; from_dir++,to_dir++) {           
        if (1 & *to_dir)
            printk("copy_page_tables: already exist"); // 目的页目录项已存在页目录项
        if (!(1 & *from_dir))
            continue;                                 // 源目录无页表则跳过

        from_page_table = (unsigned long *) (0xfffff000 & *from_dir); // from_dir指向的源页表基址
        if (!(to_page_table = (unsigned long *) get_free_page()))
            return -1;                                // 申请目标页表失败
        
        *to_dir = ((unsigned long) to_page_table) | 7; // 设置目标目录项（存在/可写/用户）
        nr = (from==0)?0xA0:1024;                      // 如果 from 为 0，则只复制内核前 160 项
        
        for ( ; nr-- > 0 ; from_page_table++,to_page_table++) { //根据nr的大小一一复制页表项
            this_page = *from_page_table;              // 读取源页表项
            if (!this_page)
                continue;                             // 空项跳过
            if (!(1 & this_page))
                continue;                             // 通过最后一位 判断页是否存在，不存在则跳过

            this_page &= ~2;                           // 清写保护位，目标页设为只读/共享映射
            *to_page_table = this_page;               // 将页表项复制到目标

            if (this_page > LOW_MEM) {                 // 高于 LOW_MEM 表示是可释放的物理页
                *from_page_table = this_page;         // 把父进程原本的页表项也改回只读
                this_page -= LOW_MEM;                // 转换为物理页号
                this_page >>= 12;
                mem_map[this_page]++;               // 增加物理页引用计数
            }
        }
    }
    invalidate();                                  // 刷新 TLB
    return 0;                                      // 成功
}
/// @brief 初始化物理内存页框管理 @param start_mem 主内存区域的起始物理地址s @param end_mem 物理内存的结束地址（最高地址）
void mem_init(long start_mem, long end_mem) {
    int i;

    HIGH_MEMORY = end_mem;  /* 保存物理内存最高地址 */
    
    for (i = 0; i < PAGING_PAGES; i++) {    /* 初始时，假设所有物理页框都已被占用（USED 为非零值） */
        mem_map[i] = USED;
    }
    /* 计算主内存起始地址对应的页框号（物理地址右移12位） */
    i = MAP_NR(start_mem);  /* 主内存区域字节数 */
    end_mem -= start_mem;   /* 主内存区域页框数 */
    end_mem >>= 12;
    while (end_mem--) { /* 将主内存区域对应的页框标记为空闲（0） */
        mem_map[i++] = 0;
    }

}

