extern int sys_fork();  // 一系统调用函数，负责创建一个新的进程（子进程），并返回子进程的 PID 给父进程。

fn_ptr sys_call_table[] = { // 系统调用表，包含了所有系统调用函数的指针。
    sys_fork,
};

int NR_syscalls = sizeof(sys_call_table)/sizeof(fn_ptr);

