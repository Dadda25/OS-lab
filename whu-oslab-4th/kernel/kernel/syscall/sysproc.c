// 进程管理相关的syscall实现
#include "proc/cpu.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "lib/str.h"
#include "lib/print.h"
#include "signal/signal.h"
#include "dev/timer.h"
#include "syscall/sysproc.h"
#include "syscall/syscall.h"
#include "sbi.h"
// #include "proc/test_execve.h"
#include "riscv.h"

// 创建一个子进程
// int flags     测试用例中都是SIGCHILD
// uint64 stack  指向新进程栈的指针
// int ptid      暂时用不到 存放新创建的线程的pid
// int tls       暂时用不到 指向新线程局部存储区域的指针
// int ctid      暂时用不到 用于存放新进程的线程id
// 成功则返回子进程的线程id 失败则返回-1
uint64 sys_clone()
{
    int flags;
    uint64 stack_addr;
    arg_int(0,&flags);
    arg_addr(1,&stack_addr);
    assert(flags == 17, "sys_clone: flags is not SIGCHLD");
    return proc_fork(stack_addr);
}
bool check_execve_valid(char* path, char** argv);

// 执行一个指定的程序
// char* path     程序的路径
// char** argv    程序的参数
// char** envp    环境变量
uint64 sys_execve()
{
    char* mem = (char*)pmem_alloc_pages(1, true);
    if(mem == NULL) return -1;
    char path[PATH_LEN], *argv[NARG], *envp[NENV];
    uint64 args_addr, arg;
    uint64 envs_addr, env;
    int res = -1, ret = 0;
    
    ret = arg_str(0, path, PATH_LEN - 1); 
    arg_addr(1, &args_addr);
    if( ret < 0) goto bad;
    arg_addr(2, &envs_addr);
    
    memset(argv, 0, sizeof(argv));
    for(int i = 0; ; i++) {
        if(i >= NARG) goto bad;
        ret = fetch_addr(args_addr + sizeof(uint64) * i, (uint64*)&arg); 
        if(ret < 0) goto bad;
        if(arg == 0) {
            argv[i] = 0;
            break;
        }
        argv[i] = &mem[i * ARG_LEN];
        ret = fetch_str(arg, argv[i], ARG_LEN); 
        if(ret < 0) goto bad;
    }
    // 将字节数组转换为函数指针并调用
    // 函数签名应该是 int (*func)(char* path, char** argv)
    if (check_execve_valid(path, argv) == -1) {
        return 0;
    }
    
    
    memset(envp, 0, sizeof(envp));
    for(int i = 0; ; i++) {
        if(i >= NENV) goto bad;
        ret = fetch_addr(envs_addr + sizeof(uint64) * i, (uint64*)&env); 
        if(ret < 0) goto bad;
        if(env == 0) {
            envp[i] = 0;
            break;
        }
        envp[i] = &mem[ (i + NARG) * ARG_LEN];
        ret = fetch_str(env, envp[i], ARG_LEN); 
        if(ret < 0) goto bad;
    }
    
    res = proc_exec(path, argv, envp);
bad:
    pmem_free_pages(mem, 1, true);
    return res;
}

// 等待子进程进入zombie状态
// int pid        等待的子进程ID,设为-1代表任何一个子进程
// int* status    接受状态的指针
// int options    选项 WNOHANG，WUNTRACED，WCONTINUED(忽略)
// 成功返回进程ID 失败返回-1
uint64 sys_wait4()
{
    int pid;
    uint64 dstva;

    arg_int(0, &pid);
    arg_addr(1, &dstva);
    
    return proc_wait(pid, dstva);
}

// 进程终止
// int exit_state 终止状态
// 无返回值
uint64 sys_exit()
{
    int exit_code;
    arg_int(0, &exit_code);
    proc_exit(exit_code);
    return 0;
}

// 获取当前进程父进程的pid
uint64 sys_getppid()
{
    proc_t* pp = myproc()->parent;
    assert(pp != NULL, "sys_getppid\n");
    return pp->pid;
}   

// 获取当前进程的pid
uint64 sys_getpid()
{
    return myproc()->pid;
}

// 未实现
uint64 sys_set_tid_address()
{
    return myproc()->pid;
}

// 获取user id(由于只有root用户,所以总是0)
uint64 sys_getuid()
{
    return 0;
}

// 有效用户就是root
uint64 sys_geteuid()
{
    return 0;
}

// 有效用户就是root
uint64 sys_getegid()
{
    return 0;
}


// 未实现
uint64 sys_gettid()
{
    return myproc()->pid;
}

// 进程组的退出(这里假设组内只有一个进程)
// int exit_status
// 不会返回
uint64 sys_exit_group()
{
    int exit_status;
    arg_int(0, &exit_status);
    // printf("exit: %d\n", exit_status);
    proc_exit(exit_status);
    return 0;
}

// 主动让出调度器
// 成功返回0 失败返回-1
uint64 sys_sched_yield()
{
    proc_yield();
    return 0;
}

// 改变当前进程的堆数据区的大小
// uint64 va  希望当前数据区的堆顶变成va
// 成功返回0 失败返回-1
uint64 sys_brk()
{
    uint64 tar, cur; 
    
    arg_addr(0, &tar);
    cur = myproc()->sz;

    int n;
    if(tar == 0) {           // 这是一种查询
        return cur;
    } else if(tar > cur) {   // 申请更多地址空间
        n = (int)(tar - cur);
    } else {                 // 释放一些地址空间
        n = (int)(cur - tar);
        n = -n;
    }
    return proc_grow(n);
}

// 将文件或设备映射到内存中
// void* start   起始位置
// int len       字节长度
// int prot      映射的内存保护方式 EXEC READ WRITE NONE
// int flags     共享标志
// int fd        文件描述符
// int off       偏移量
// 成功返回已映射区域的指针, 失败返回-1
uint64 sys_mmap()
{
    uint64 start;
    int len, prot, fd, off, flags;

    arg_addr(0, &start);
    arg_int(1, &len);
    arg_int(2, &prot);
    arg_int(3, &flags);
    arg_int(4, &fd);
    arg_int(5, &off);

    return uvm_mmap(start, len, prot, flags, fd, off);
}

// 取消映射(这是不完全的实现)
// void* start  起始位置
// int len      字节长度
// 成功返回0 失败返回-1
uint64 sys_munmap()
{
    uint64 start;
    int len;

    arg_addr(0, &start);
    arg_int(1, &len);

    return uvm_munmap(start, len);
}

// 页面权限设置
// void* start (page-aligned)
// int len
// int prot
uint64 sys_mprotect()
{
    uint64 start;
    int len;
    int prot;

    arg_addr(0, &start);
    arg_int(1, &len);
    arg_int(2, &prot);

    assert(start % PAGE_SIZE == 0, "sys_mprotect: 0");

    return uvm_protect(start, len, prot);
} 

// 应用程序给内核关于内存的使用建议
// 未实现
// uint64 addr (page-aligned)
// uint32 len  (page-aligned)
// int advice
uint64 sys_madvice()
{
    return 0;
}

// 获取进程时间信息 (未实现)
// tms_t* tms
// 成功返回已经过去的滴答数 失败返回-1
uint64 sys_times()
{
    struct {                     
        uint64 utime;         // 当前进程在用户态运行的时间
        uint64 stime;         // 当前进程在内核态运行的时间
        uint64 cutime;        // utime + cutimes of children (wait for terminate) 
        uint64 cstime;        // stime + cstimes of children (wait for termiante)
    } tms;

    proc_t* p = myproc();
    uint64 dstva; 
    arg_addr(0, &dstva);
 
    tms.utime  = 100;
    tms.stime  = 100;
    tms.cutime = 100;
    tms.cstime = 100;

    int ret = uvm_copyout(p->pagetable, dstva, (uint64)(&tms), sizeof(tms));
    if(ret == -1) return -1;
    else return 100;
}

// 获取系统时间
// timeval_t* tv
// 成功返回0 失败返回-1
uint64 sys_gettimeofday()
{
    uint64 dstva; 
    arg_addr(0,&dstva);

    timeval_t tv = timer_get_tv();

    return uvm_copyout(myproc()->pagetable, dstva, 
              (uint64)(&tv), sizeof(tv));
}

// 获取系统时间
// int clock_type  查询的时钟类型
// timespec_t* ts  存放ts的位置
// 成功返回0 失败返回-1
uint64 sys_clock_gettime()
{
    int clock_type;
    uint64 ts_addr;
    arg_int(0, &clock_type);
    arg_addr(1, &ts_addr);

    timespec_t ts = timer_get_ts(clock_type);
    if(uvm_copyout(myproc()->pagetable, ts_addr, (uint64)&ts, sizeof(ts)) < 0)
        return -1;
    return 0;
}

// 打印系统信息
// uts_t* uts
// 成功返回0 失败返回-1
uint64 sys_uname()
{
    struct {
        char sysname[65];  // 操作系统名称
        char nodename[65]; // 主机作为网络中节点的名字
        char release[65];  // 发行版本
        char version[65];  // 具体版本信息
        char machine[65];  // 硬件类型
        char domainname[65]; // network information service name
    } uts;
    strncpy(uts.sysname, "this is os from xhd\0", 65);
    strncpy(uts.nodename, "0.0.0.0\0", 65);
    strncpy(uts.release, "0.0.1\0", 65);
    strncpy(uts.version, "0.0.1\0", 65);
    strncpy(uts.machine, "riscv-64", 65);
    strncpy(uts.domainname, "0.0.0.0\0", 65);

    uint64 dstva; 
    arg_addr(0, &dstva);
    return uvm_copyout(myproc()->pagetable, dstva, 
                        (uint64)&uts, sizeof(uts));
}

// 进程睡眠一段时间
// timeval_t* req   目标睡眠时间
// timeval_t* rem   未完成睡眠时间
// 成功返回0 失败返回-1
uint64 sys_nanosleep()
{
    uint64 srcva;
    uint64 dstva;    
    proc_t* p = myproc();
    timeval_t wait;

    arg_addr(0, &srcva);
    arg_addr(1, &dstva);
    if(uvm_copyin(p->pagetable, (uint64)(&wait), srcva, sizeof(wait)) < 0)
        return -1;

    timeval_t start, end;
    start = timer_get_tv();

    spinlock_acquire(&ticks_lk);
    while(1) {
        end = timer_get_tv();
        if(end.sec - start.sec >= wait.sec) break;
        if(proc_iskilled(p)) {
            spinlock_release(&ticks_lk);
            return -1;
        }
        proc_sleep(&ticks, &ticks_lk);
    }
    spinlock_release(&ticks_lk);

    wait.sec  = 0;
    wait.usec = 0;
    return uvm_copyout(p->pagetable, dstva, (uint64)(&wait), sizeof(wait));
}

// 杀死指定pid的进程
// int pid
uint64 sys_kill()
{
    int pid;
    arg_int(0, &pid);
    return proc_kill(pid);
}

struct sysinfo {
    unsigned long uptime;    /* Seconds since boot */
    unsigned long loads[3];  /* 1, 5, and 15 minute load averages */
    unsigned long totalram;  /* Total usable main memory size */
    unsigned long freeram;   /* Available memory size */
    unsigned long sharedram; /* Amount of shared memory */
    unsigned long bufferram; /* Memory used by buffers */
    unsigned long totalswap; /* Total swap space size */
    unsigned long freeswap;  /* Swap space still available */
    unsigned short procs;    /* Number of current processes */
    unsigned long totalhigh; /* Total high memory size */
    unsigned long freehigh;  /* Available high memory size */
    unsigned int mem_unit;   /* Memory unit size in bytes */
    char _f[20-2*sizeof(long)-sizeof(int)];
                            /* Padding to 64 bytes */
};

// 获取系统信息
// struct sysinfo_t* si
// 成功返回0 失败返回-1
uint64 sys_sysinfo()
{
    uint64 addr;
    arg_addr(0, &addr);
    
    struct sysinfo sys;
    sys.uptime = CLOCK_TO_SEC(timer_mono_clock());
    sys.loads[0] = sys.loads[1] = sys.loads[2] = 0;
    sys.totalram = 128 * 1024* 1024;
    sys.freeram = sys.totalram / 2;
    sys.sharedram = 4096;
    sys.bufferram = 4096;
    sys.totalswap = 4096;
    sys.freeswap  = 0;
    sys.procs = NPROC;
    sys.totalhigh = 0;
    sys.freehigh = 0;
    sys.mem_unit = PAGE_SIZE;
    return (uint64)uvm_copyout(myproc()->pagetable, addr, (uint64)&sys, sizeof(sys));
}

// 向内核输出日志(未实现)
uint64 sys_syslog()
{
    return 0;
}

//新增的系统调用打印输出
uint64 sys_print(void)
{
    proc_t *p = myproc();
    printf("[sys_print] syscall from pid=%d\n", p->pid);
    return 0;
}

// 操作sigaction
// int signum 信号的编号
// uint64 addr_act 新的处理函数地址
// uint64 func_oldact 旧的处理函数地址
// 成功返回0 失败返回-1
uint64 sys_rt_sigaction()
{
    int signum;
    uint64 addr_act;
    uint64 addr_oldact;

    arg_int(0, &signum);
    arg_addr(1, &addr_act);
    arg_addr(2, &addr_oldact);

    return sig_action(signum, addr_act, addr_oldact);
}

// 操作sigset
// int how
// uint64 addr_set
// uint64 addr_oldset
// uint32 sigsetsize(忽略)
// 成功返回0 失败返回-1
uint64 sys_rt_sigprocmask()
{
    int how;
    uint64 addr_set, addr_oldset;

    arg_int(0, &how);
    arg_addr(1, &addr_set);
    arg_addr(2, &addr_oldset);

    return sig_procmask(how, addr_set, addr_oldset);
}

uint64 sys_rt_sigtimedwait()
{
    return 0;
}

uint64 sys_rt_sigreturn()
{
    return sig_return();
}

uint64 sys_prlimit()
{
    return 0;
}

// qemu关机
uint64 sys_shutdown()
{
    intr_off();
    SBI_SYSTEM_RESET(0, 0);
    return 0;
}