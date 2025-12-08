#include "lib/print.h"         // printf
#include "proc/cpu.h"          // myproc
#include "mem/vmem.h"          // uvm_copy
#include "syscall/sysnum.h"
#include "syscall/sysfile.h"
#include "syscall/sysproc.h"

static uint64 (*syscalls[])(void) = {
    // 文件操作
    [SYS_getcwd]           sys_getcwd,
    [SYS_getdents64]       sys_getdents64,
    [SYS_mkdirat]          sys_mkdirat,
    [SYS_chdir]            sys_chdir,
    [SYS_openat]           sys_openat,
    [SYS_close]            sys_close,
    [SYS_lseek]            sys_lseek,
    [SYS_read]             sys_read,
    [SYS_write]            sys_write,
    [SYS_readv]            sys_readv,
    [SYS_writev]           sys_writev,
    [SYS_pread64]          sys_pread64,
    [SYS_pwrite64]         sys_pwrite64,
    [SYS_pipe2]            sys_pipe2,
    [SYS_dup]              sys_dup,
    [SYS_dup2]             sys_dup2,
    [SYS_linkat]           sys_linkat,
    [SYS_unlinkat]         sys_unlinkat,
    [SYS_mount]            sys_mount,
    [SYS_umount2]          sys_umount2,
    [SYS_fstat]            sys_fstat,
    [SYS_fstatat]          sys_fstatat,
    [SYS_faccessat]        sys_faccessat,
    [SYS_statfs]           sys_statfs,
    [SYS_utimensat]        sys_utimensat,
    [SYS_sendfile]         sys_sendfile,
    [SYS_fcntl]            sys_fcntl,
    [SYS_ioctl]            sys_ioctl,
    [SYS_renameat2]        sys_renameat2,
    // 进程操作
    [SYS_clone]            sys_clone,
    [SYS_execve]           sys_execve,
    [SYS_wait4]            sys_wait4,
    [SYS_exit]             sys_exit,
    [SYS_exit_group]       sys_exit_group,
    [SYS_getppid]          sys_getppid,
    [SYS_getpid]           sys_getpid,
    [SYS_getuid]           sys_getuid,
    [SYS_geteuid]          sys_geteuid,
    [SYS_getegid]          sys_getegid,
    [SYS_gettid]           sys_gettid,
    [SYS_set_tid_address]  sys_set_tid_address, 
    [SYS_sched_yield]      sys_sched_yield,
    [SYS_nanosleep]        sys_nanosleep,
    [SYS_kill]             sys_kill,
    // 内存操作
    [SYS_brk]              sys_brk,
    [SYS_mmap]             sys_mmap,
    [SYS_munmap]           sys_munmap,
    [SYS_mprotect]         sys_mprotect,
    [SYS_madvice]          sys_madvice,
    // 信号相关
    [SYS_rt_sigaction]     sys_rt_sigaction,
    [SYS_rt_sigprocmask]   sys_rt_sigprocmask,
    [SYS_rt_sigtimedwait]  sys_rt_sigtimedwait,
    [SYS_rt_sigreturn]     sys_rt_sigreturn,
    // 其他
    [SYS_times]            sys_times,
    [SYS_gettimeofday]     sys_gettimeofday,
    [SYS_clock_gettime]    sys_clock_gettime,
    [SYS_uname]            sys_uname,
    [SYS_sysinfo]          sys_sysinfo,
    [SYS_syslog]           sys_syslog,
    [SYS_prlimit]          sys_prlimit,
    [SYS_ppoll]            sys_ppoll,
    [SYS_print]            sys_print,//新增的系统调用打印输出
    // 关机指令
    [SYS_shutdown]         sys_shutdown,
};

/*
    执行目标syscall函数
    将函数返回值放在寄存器a0中
    在trap_user.c中调用
*/
void syscall() 
{
    proc_t* p = myproc();

    // 按照系统调用约定,syscall num存放在tf->a7中
    int num = p->tf->a7;
    if(num > 0 && num < 500 && syscalls[num]) { // 合法的syscall num
        // syscalls[num]对应的函数作为返回值放在tf->a0
        // printf("begin pid = %d num = %d\n", p->pid, num);
        p->tf->a0 = syscalls[num]();
        // printf("end pid = %d num = %d return = %uld\n",p->pid, num, p->tf->a0);
    } else { // 非法的syscall num
        printf("unknown syscall %d from pid = %d\n",num,p->pid);
        panic("syscall");
    }
}

/*
    其他用于读取传入参数的函数
    参数分为两种,第一种是数据本身,第二种是指针
    第一种使用tf->ax传递
    第二种使用uvm_copyin 和 uvm_copyin2 进行传递
*/

static bool legal_addr(uint64 addr) {
    proc_t* p = myproc();
    vm_region_t* vm = p->vm_head;

    if(addr + sizeof(uint64) <= p->sz)
        return true;
    while (vm != NULL) {
        if(vm->start <= addr) {
            if(vm->start + PAGE_SIZE >= addr + sizeof(uint64)) {
                return true;
            }
        }
        vm = vm->next;
    }
    return false;
} 

// 拿到一个地址为 addr 的64位整数,让 ip 指向它
// 成功返回0,失败返回-1
int fetch_addr(uint64 addr, uint64* ip)
{
    proc_t* p = myproc();

    // 检查地址合法性
    if(legal_addr(addr) == false)
        return -1;

    // 尝试拷贝用户态数据
    if(uvm_copyin(p->pagetable, (uint64)ip ,addr, sizeof(*ip)) != 0)
        return -1;
    
    return 0;
}  

// 将地址为addr字符串复制到buf中, 限制字符串的最大长度maxlen
// 成功返回0,失败返回-1
int fetch_str(uint64 addr, char* buf, int maxlen)
{
    proc_t* p = myproc();
    if(uvm_copyin2(p->pagetable, buf, addr, maxlen) != 0)
        return -1;
    return 0;
}

// 读取 n 号参数,它放在 an 寄存器中
static uint64 arg_raw(int n)
{   
    proc_t* proc = myproc();
    switch(n) {
        case 0:
            return proc->tf->a0;
        case 1:
            return proc->tf->a1;
        case 2:
            return proc->tf->a2;
        case 3:
            return proc->tf->a3;
        case 4:
            return proc->tf->a4;
        case 5:        
            return proc->tf->a5;
        default:
            panic("syscall.c->argraw: illegal arg num");
            return -1;
    }
}

// 读取 n 号参数,作为int32存入 ip
void arg_int(int n, int* ip)
{
    *ip = arg_raw(n);
}

// 读取 n 号参数,作为uint64存入 ip
void arg_addr(int n, uint64* ip)
{
    *ip = arg_raw(n);
}

// 将 n 号参数指向的字符串读入 buf 中,字符串最长长度是 maxlen
// 若读取成功返回字符串长度,若失败返回-1
int arg_str(int n, char* buf, int maxlen)
{
    // 从寄存器中读出虚拟地址
    uint64 addr;
    arg_addr(n, &addr);
    // 根据虚拟地址找到这个字符串
    return fetch_str(addr, buf, maxlen);
}