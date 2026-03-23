#include <errno.h>

#include <asm/system.h>
#include <linux/sched.h>
#include <string.h>

long last_pid = 0;
// 复制当前进程的内存空间，设置新进程的LDT和页表
int copy_mem(int nr, struct task_struct* p) {   
    unsigned long old_data_base,new_data_base,data_limit; // 旧/新 数据段基址和数据段界限
    unsigned long old_code_base,new_code_base,code_limit; // 旧/新 代码段基址和代码段界限

    code_limit = get_limit(0x0f);           // 取代码段界限(LDT/描述符索引 0x0f)
    data_limit = get_limit(0x17);           // 取数据段界限(LDT/描述符索引 0x17)
    old_code_base = get_base(current->ldt[1]); // 取当前进程的代码段基址
    old_data_base = get_base(current->ldt[2]); // 取当前进程的数据段基址
    if (old_data_base != old_code_base)
        panic("We don't support separate I&D"); // 不支持分离的 I&D 段
    if (data_limit < code_limit)
        panic("Bad data_limit");             // 数据界限应不小于代码界限

    new_data_base = new_code_base = nr * TASK_SIZE; // 新进程的线性基址
    set_base(p->ldt[1],new_code_base);       // 设置子进程 LDT 的代码基址
    set_base(p->ldt[2],new_data_base);       // 设置子进程 LDT 的数据基址
    if (copy_page_tables(old_data_base,new_data_base,data_limit)) { // 复制页表
        free_page_tables(new_data_base,data_limit); // 复制失败则释放新页表
        return -ENOMEM;                         // 返回内存不足错误
    }

    return 0; // 成功
}
// 创建一个新的进程（子进程），并返回子进程的 PID 给父进程
int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
        long ebx,long ecx,long edx, long orig_eax,
        long fs,long es,long ds,
        long eip,long cs,long eflags,long esp,long ss) {// 复制当前进程的上下文信息，创建一个新的进程（子进程），并返回子进程的 PID 给父进程
    struct task_struct *p;

    p = (struct task_struct *) get_free_page(); // 获取一个新的页
    if (!p)               
        return -EAGAIN;    

    task[nr] = p;         // 在任务表中登记新任务
    memcpy(p, current, sizeof(struct task_struct)); // 复制当前进程的 task_struct

    p->pid = last_pid;    // 设置子进程 pid
    p->p_pptr = current;   // 设置父进程指针

    p->tss.back_link = 0;                         // TSS 链接域
    p->tss.esp0 = PAGE_SIZE + (long)p;        // 内核栈指针
    p->tss.ss0 = 0x10;                            // 内核数据段选择子
    p->tss.cr3 = current->tss.cr3;                // 复制页目录（地址空间）
    p->tss.eip = eip;                             // 用户态指令指针
    p->tss.eflags = eflags;                       // 标志寄存器
    p->tss.eax = 0;                               // 子进程 fork 返回值为 0
    p->tss.ecx = ecx;                             // 保存通用寄存器上下文
    p->tss.edx = edx;
    p->tss.ebx = ebx;
    p->tss.esp = esp;
    p->tss.ebp = ebp;
    p->tss.esi = esi;
    p->tss.edi = edi;
    p->tss.es  = es & 0xffff;                     // 段寄存器截取 16 位选择子
    p->tss.cs  = cs & 0xffff;
    p->tss.ss  = ss & 0xffff;
    p->tss.ds  = ds & 0xffff;
    p->tss.fs  = fs & 0xffff;
    p->tss.gs  = gs & 0xffff;
    p->tss.ldt = _LDT(nr);                        // LDT 表项
    p->tss.trace_bitmap = 0x80000000;             // 禁用 IO 位图

    if (copy_mem(nr, p)) {    // 复制父进程内存空间，失败则清理并返回
        task[nr] = NULL;
        free_page((long)p);
        return -EAGAIN;
    }

    set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss));
    set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));

    return last_pid;
}
//寻找一个空的任务槽，返回其索引，如果没有空闲槽则返回错误码
int find_empty_process() {
    int i;
    repeat:
    if ((++last_pid)<0) last_pid=1;

    for(i=0 ; i<NR_TASKS ; i++) {
        if (task[i] && (task[i]->pid == last_pid))
            goto repeat;
    }

    for (i = 1; i < NR_TASKS; i++) {
        if (!task[i])
            return i;
    }

    return -EAGAIN;
}

