#ifndef __PROC_H__
#define __PROC_H__

#include "lock/lock.h"
#include "signal/signal.h"

typedef uint64* pgtbl_t;
typedef struct vm_region vm_region_t;
typedef struct fat32_inode fat32_inode_t;
typedef struct ext4_inode ext4_inode_t;
typedef struct fat32_file fat32_file_t;
typedef struct ext4_file ext4_file_t;

// 用户态数据暂存 + 内核态信息传递
typedef struct trapframe {
    uint64 kernel_satp;       // 内核页表信息
    uint64 kernel_sp;         // 内核stack pointer
    uint64 kernel_trap;       // trap_user()地址
    uint64 kernel_hartid;     // 来自tp寄存器
    uint64 epc;               // 这里trap发生时保存用户态地址
    reg ra;
    reg sp;
    reg gp;
    reg tp;
    reg t0;
    reg t1;
    reg t2;
    reg s0;
    reg s1;
    reg a0;
    reg a1;
    reg a2;
    reg a3;
    reg a4;
    reg a5;
    reg a6;
    reg a7;
    reg s2;
    reg s3;
    reg s4;
    reg s5;
    reg s6;
    reg s7;
    reg s8;
    reg s9;
    reg s10;
    reg s11;
    reg t3;
    reg t4;
    reg t5;
    reg t6;
} trapframe_t;

// 进程上下文
typedef struct context {
    reg ra; 
    reg sp;
    reg s0;
    reg s1;
    reg s2;
    reg s3;
    reg s4;
    reg s5;
    reg s6;
    reg s7;
    reg s8;
    reg s9;
    reg s10;
    reg s11;
} context_t;

typedef enum procstate {
    UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE
} procstate_t;

typedef struct proc{

    spinlock_t lk;        // 保证数据一致性的锁

    /* 进程本身 (查看或修改时需持有lk) */
    int pid;              // id号
    procstate_t state;    // 进程状态
    void* channel;        // 进程休眠的地方
    bool killed;          // 是否要exit
    int exit_state;       // 退出时的信息

    /* 父进程 (查看或修改时需持有parent_lock) */
    struct proc* parent;  // 父进程
    
    /* 内存相关 */
    uint64 sz;            // 静态区域 + 用户栈[0,sz]
    uint64 vm_allocable;  // 指向一个可以分配给mmap的虚拟地址 (page_aligned) 
    vm_region_t* vm_head; // mmap管理的双向循环链表
    uint64 kstack;        // 内核栈地址
    context_t ctx;        // 用于swtch.S
    trapframe_t* tf;      // 用于trampoline.S
    pgtbl_t pagetable;    // 用户页表
    
    /* 文件相关 */
    fat32_inode_t* fat32_cwd;          // 当前目录
    fat32_file_t* fat32_ofile[NOFILE]; // 打开文件列表

    ext4_inode_t*  ext4_cwd;           // 当前目录
    ext4_file_t*  ext4_ofile[NOFILE];  // 打开文件列表

    /* 信号相关 */
    sigaction_t sigactions[NSIG];
    sigset_t    sig_pending;
    sigset_t    sig_set;
    sigframe_t* sig_frame;

} proc_t;


// 内存相关

int     proc_grow(int n);
pgtbl_t proc_alloc_pagetable(proc_t* p);
void    proc_destroy_pagetable(pgtbl_t pagetable, uint64 sz, vm_region_t* vm_head);
void    proc_mapstacks(pgtbl_t pagetable);

// 调度相关

void proc_sched(void);
void proc_schedule(void);

// 进程生命周期

void proc_userinit(void);
int  proc_exec(char* path, char** argv, char** envp);
int  proc_fork(uint64 stack);
void proc_exit(int status);
int  proc_wait(int pid, uint64 addr);
void proc_sleep(void* channel, spinlock_t* lock);
void proc_wakeup(void* channel);
int  proc_kill(int pid);
void proc_yield(void);

// 其他函数

void proc_init(void);
bool proc_iskilled(proc_t* p);
void proc_setkilled(proc_t* p);
void proc_reparent(proc_t* p);
void proc_print(void);
void proc_show_pgtbl();

#endif

/*
------------------------------------- VA_MAX
        trampoline 代码区
------------------------------------- VA_MAX - 4KB
        trapframe  数据区
------------------------------------- VA_MAX - 8KB


        可分配区域


------------------------------------- proc->sz
        已分配的数据区域
------------------------------------- 4KB
        用户栈    

        (空闲区域)
        
        代码和静态数据区域
------------------------------------- 0
*/