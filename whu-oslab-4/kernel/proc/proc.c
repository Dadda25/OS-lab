// 进程管理
#include "lib/str.h"
#include "lib/print.h"
#include "proc/cpu.h"
#include "proc/initcode.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "trap/trap.h"
#include "fs/fat32.h"
#include "fs/fat32_inode.h"
#include "fs/fat32_dir.h"
#include "fs/fat32_file.h"
#include "fs/ext4.h"
#include "fs/ext4_inode.h"
#include "fs/ext4_dir.h"
#include "fs/ext4_file.h"
#include "dev/timer.h"
#include "dev/console.h"
#include "riscv.h"

static proc_t procs[NPROC];     // 保存进程信息的数组
static proc_t* initproc;        // 第一个用户态进程,永不退出 
static spinlock_t parent_lock;  // 在涉及进程父子关系时使用

extern char trampoline[];                          // in trampoline.S
extern void swtch(context_t* old, context_t* new); // in Swtch.S

extern fat32_inode_t fat32_rooti;
extern ext4_inode_t ext4_rooti;

// pid和保护它的锁
static int nextpid = 1;
static spinlock_t pid_lock;

// 辅助函数声明
static int alloc_pid();
static proc_t* alloc_proc();
static void free_proc(proc_t* p);
void forkret(void);

/* --------------------------------------辅助函数----------------------------------- */

/*
    获取可用的pid号
*/
static int alloc_pid()
{
    spinlock_acquire(&pid_lock);
    int pid = nextpid++;
    spinlock_release(&pid_lock);
    return pid;
}

/* 
    在procs列表中寻找一个未被使用的空间
    若成功则返回这个可用的proc,若失败则返回NULL
    注意:若成功执行,得到的空闲proc是上了锁的
*/
static proc_t* alloc_proc()
{
    proc_t* p;

    for(p = procs; p < procs + NPROC; p++) {
        spinlock_acquire(&p->lk);
        if(p->state == UNUSED)
            goto success;
        spinlock_release(&p->lk);
    }
    return NULL;

success:

    // 申请一页作为trapframe的物理地址空间
    p->tf = (trapframe_t*)pmem_alloc_pages(1, true);
    if(p->tf == NULL) goto fail;

    // 申请一个pagetable并完成trapframe和trampoline的映射
    p->pagetable = proc_alloc_pagetable(p);
    if(p->pagetable == NULL) goto fail;
    p->vm_allocable = VM_MMAP_START;
    p->vm_head = NULL;
    
    // 设置上下文
    memset(&p->ctx, 0, sizeof(p->ctx)); 
    p->ctx.ra = (uint64)forkret;         // 返回地址
    p->ctx.sp = p->kstack + PAGE_SIZE;   // 内核栈顶
    
    // 设置pid和state
    p->pid = alloc_pid();
    p->state = USED;

    return p;

fail:
    free_proc(p);
    spinlock_release(&p->lk);
    return NULL;
}

/* 
    与alloc_proc相对应
    使得p重新进入UNUSED干净状态,同时释放申请的物理页
    注意：调用者应持有p的锁
*/
static void free_proc(proc_t* p)
{
    assert(spinlock_holding(&p->lk), "proc.c->free_proc: 1\n");
    
    // 与proc_alloc的步骤相对应
    if(p->tf) { 
        pmem_free_pages((void*)p->tf, 1, true);
        p->tf = NULL;
    }
    if(p->pagetable) { 
        proc_destroy_pagetable(p->pagetable, p->sz, p->vm_head);
        p->pagetable = NULL;
    }
    p->pid = 0;
    p->state = UNUSED;
    
    // 其他字段的清零
    p->sz = 0;
    p->vm_head = NULL;
    p->vm_allocable = 0;
    p->parent = NULL;
    p->channel = NULL;
    p->killed = false;
    p->exit_state = 0;
}


/* -------------------------------------接口函数--------------------------------- */

/*
    这个函数负责解锁，然后调用trapret_user返回用户态
*/
static bool first = true;

void forkret(void)
{
    spinlock_release(&myproc()->lk);
    
    // 文件系统初始化
    if(first) {
        first = false;
#ifdef FS_FAT32
        fat32_init(0, 0);
#else
        ext4_init(0, 2);
#endif
    }

    // 返回用户态
    trapret_user();
}


/*
    myproc->sz += n n可正可负 
    增加或减少进程控制的物理页
    成功返回0, 失败返回-1
*/ 
int proc_grow(int n)
{
    proc_t* proc = myproc();
    uint64 newsz = proc->sz, oldsz = proc->sz;

    if(n > 0) {
        newsz = uvm_grow(proc->pagetable, oldsz, oldsz + n, PTE_W | PTE_R);
        if(newsz != oldsz + n) return -1;
    } else if(n < 0) {
        newsz = uvm_ungrow(proc->pagetable, oldsz, oldsz + n);
        if(newsz != oldsz + n) return -1; 
    }

    proc->sz = newsz;
    return 0;
}

/* 
    对进程列表中每个进程的kstack进行地址映射
    kstack的映射一旦建立就不会修改
    (该函数只在初始化过程中调用一次)
*/
void proc_mapstacks(pgtbl_t pagetable)
{
    int ret;
    uint64 va, pa;

    for(proc_t* p = procs; p < procs + NPROC; p++) {
    
        // 申请物理页,得到pa
        pa = (uint64)pmem_alloc_pages(1, true);
        assert(pa != 0, "proc.c->proc_mapstacks: 1\n");
    
        // 虚拟地址va是特定的,直接计算
        va = KSTACK((int)(p-procs));
    
        // 建立映射关系,这里的pagetable是内核页表
        ret = vm_mappages(pagetable, va, pa, PAGE_SIZE, PTE_R | PTE_W);
        assert(ret == 0, "proc.c->proc_mapstacks: 2\n");
    }
}

/*
    申请一个用户进程的页表,并完成trapframe和trampoline的映射
    若成功则返回页表, 若失败则返回NULL
*/
pgtbl_t proc_alloc_pagetable(proc_t* p)
{
     
    pgtbl_t pagetable = uvm_alloc_pagetable();
    if(pagetable == NULL) return NULL;

    int ret = 0;
    // 映射trampoline区域,代码区
    ret = vm_mappages(pagetable, TRAMPOLINE, (uint64)trampoline, PAGE_SIZE, PTE_R | PTE_X);
    if(ret != 0) goto fail;

    // 映射trapframe区域,数据区
    ret = vm_mappages(pagetable, TRAPFRAME, (uint64)(p->tf),PAGE_SIZE, PTE_R | PTE_W); 
    if(ret != 0) {
        uvm_unmappages(pagetable, TRAMPOLINE, 1, false);
        goto fail;
    }
    
    return pagetable;

fail:
    // 与uvm_proc_alloc_pagetable相对应,释放申请到的页表 
    uvm_free_pagetable(pagetable);
    return NULL;  
}

/*
    proc_alloc_pagetable的逆过程
    解除trapframe和trampoline的映射,释放页表
*/
void proc_destroy_pagetable(pgtbl_t pagetable, uint64 sz, vm_region_t* vm_head)
{
    // 注意: trapframe所占物理页的释放应该在外部
    // trampoline属于代码区域根本不应释放
    uvm_unmappages(pagetable, TRAMPOLINE, 1, false);
    uvm_unmappages(pagetable, TRAPFRAME, 1, false);
    uvm_free(pagetable, sz, vm_head);
}


/* 
    做一些检查,然后swtch-to调度器
    控制流转变: current proc -> cpu's scheduler
    注意: 调用者应当持有进程锁, 中断应该处于关闭状态
*/ 
void proc_sched()
{
    proc_t* proc = myproc();

    // 上锁状态 + 非RUNNING + 中断关闭
    assert(spinlock_holding(&proc->lk),"proc.c->proc_sched: 1\n");
    assert(proc->state != RUNNING, "proc.c->proc_sched: 2\n");  
    assert(intr_get() == 0, "proc.c->proc_sched: 3\n");
    
    // 暂存此时中断的开关状态, 后面恢复
    int org = mycpu()->origin;
    swtch(&proc->ctx, &mycpu()->ctx);
    mycpu()->origin = org;
}

/* 
    调度器: 选择一个新的可执行进程, 开始运行它
    控制流转变: cpu's scheduler -> selected proc
*/
void proc_schedule()
{
    // 第一次进入时执行
    proc_t* p;
    cpu_t* cpu = mycpu();

    // 此时没有进程在CPU上执行
    cpu->myproc = NULL;

    while(1) {        
        
        intr_on();
        
        for(p = procs; p < procs + NPROC; p++) {

            spinlock_acquire(&p->lk);
            // 找到准备就绪的进程了
            if(p->state == RUNNABLE) {
                p->state = RUNNING;                  
                cpu->myproc = p;
                // 切换执行流
                swtch(&cpu->ctx, &p->ctx);    
                // 返回这里时没有用户进程在CPU上执行                
                cpu->myproc = NULL;
            }
            spinlock_release(&p->lk);
        }
    }
}

/*
    初始化所有进程和用到的锁
    设置进程状态和内核栈地址
    在main.c中被调用
*/
void proc_init()
{
    spinlock_init(&pid_lock, "nextpid");
    spinlock_init(&parent_lock, "parent proc");

    for(proc_t* p = procs; p < procs + NPROC; p++) {
        spinlock_init(&p->lk, "proc");
        p->state = UNUSED;
        // 此时映射已经完成 可以赋值
        p->kstack = KSTACK((int)(p-procs));
    }
}

/*
    准备第一个进程(它的虚拟地址空间如下)
    ---------------- sp = 4K
    (free space)
    
    initcode
    ---------------- pc = 0
*/
void proc_userinit(void)
{
    // 创建initproc
    initproc = alloc_proc();
    assert(initproc != NULL, "proc_userinit: 0\n");

    uint32 len = sizeof(initcode);
    uvm_map_initcode(initproc->pagetable, initcode, len);

    initproc->sz = ALIGN_UP(len, PAGE_SIZE) + PAGE_SIZE;      // 用户地址空间大小
    initproc->tf->epc = 0;                                    // 返回用户态时的PC值        
    initproc->tf->sp = initproc->sz;                          // 栈指针
    initproc->state = RUNNABLE;

    // 文件系统相关
    // 使用设备文件console 构建STDIN STDOUT STDERR
#ifdef FS_FAT32
    fat32_file_init();
    fat32_file_t* file = fat32_file_alloc();
    file->type = FD_DEVICE;
    file->major = CONSOLE;
    file->readable = true;
    file->writable = true;
    initproc->fat32_cwd = &fat32_rooti;
    initproc->fat32_ofile[0] = file;
    initproc->fat32_ofile[1] = fat32_file_dup(file);
    initproc->fat32_ofile[2] = fat32_file_dup(file);
#else
    ext4_file_init();
    ext4_file_t* file = ext4_file_alloc();
    file->file_type = TYPE_CHARDEV;
    file->major = CONSOLE;
    file->oflags = FLAGS_RDWR;
    initproc->ext4_cwd = &ext4_rooti;
    initproc->ext4_ofile[0] = file;
    initproc->ext4_ofile[1] = ext4_file_dup(file);
    initproc->ext4_ofile[2] = ext4_file_dup(file);
#endif

    // 在alloc_proc中上了锁, 所以在这里解锁    
    spinlock_release(&initproc->lk);
} 

/*
    进程自愿放弃 CPU 使用权
    RUNNING -> RUNNABLE
*/
void proc_yield()
{
    proc_t* proc = myproc();
    spinlock_acquire(&proc->lk);
    proc->state = RUNNABLE;
    proc_sched();
    spinlock_release(&proc->lk);
}

/*
    进程p创建子进程np
    p的返回值是子进程pid
    np的返回值是0
    成功返回0, 失败返回-1
*/
int proc_fork(uint64 stack)
{
    proc_t* p = myproc();
    proc_t* np = alloc_proc();
    if(np == NULL) return -1;

    // 尝试复制p的地址空间给np
    if(uvm_copy_pagetable(p->pagetable, np->pagetable, p->sz, p->vm_head) < 0) {    
        free_proc(np);
        spinlock_release(&np->lk);
        return -1;
    }
    np->sz = p->sz;
    np->vm_allocable = p->vm_allocable;
    np->vm_head = p->vm_head;

    // 复制trapframe, np的返回值设为0, 堆栈指针设为目标堆栈
    *(np->tf) = *(p->tf);
    np->tf->a0 = 0;
    if(stack != 0) np->tf->sp = stack;

    // 打开文件表的复制和工作目录的复制
#ifdef FS_FAT32
    for(int i = 0; i < NOFILE; i++) {
        if(p->fat32_ofile[i]) {
            np->fat32_ofile[i] = fat32_file_dup(p->fat32_ofile[i]);
        }
    }
    np->fat32_cwd = fat32_inode_dup(p->fat32_cwd);
#else
    for(int i = 0; i < NOFILE; i++) {
        if(p->ext4_ofile[i]) {
            np->ext4_ofile[i] = ext4_file_dup(p->ext4_ofile[i]);
        }
    }
    np->ext4_cwd = ext4_inode_dup(p->ext4_cwd);
#endif


    int pid = np->pid;
    spinlock_release(&np->lk);

    // 修改np->parent
    spinlock_acquire(&parent_lock);
    np->parent = p;
    spinlock_release(&parent_lock);

    // 修改np->state
    spinlock_acquire(&np->lk);
    np->state = RUNNABLE;
    spinlock_release(&np->lk);
    
    return pid;
}

/*
    进程状态变化: RUNNING->ZOMBIE
    当前进程宣布即将退出, 退出状态参数为exit_state
*/
void proc_exit(int exit_state)
{
    proc_t* p = myproc();
    assert(p != initproc,"proc.c->proc_exit: 0\n");

#ifdef FS_FAT32    
    // 关闭通用文件 0 1 2
    fat32_file_t* file;
    for(int fd = 0; fd < 3; fd++) {
        if(p->fat32_ofile[fd]) {
            file = p->fat32_ofile[fd];
            fat32_file_close(file);
            p->fat32_ofile[fd] = NULL; 
        }
    }
    
    // 其他文件指针设为NULL
    for(int fd = 3; fd < NOFILE; fd++)
        p->fat32_ofile[fd] = NULL;
    
    // 关闭工作目录
    fat32_inode_put(p->fat32_cwd);
    p->fat32_cwd = NULL;
#else
    // 关闭通用文件 0 1 2
    ext4_file_t* file;
    for(int fd = 0; fd < NOFILE; fd++) {
        if(p->ext4_ofile[fd]) {
            file = p->ext4_ofile[fd];
            ext4_file_close(file);
            p->ext4_ofile[fd] = NULL; 
        }
    }
    
    // // 其他文件指针设为NULL
    // for(int fd = 3; fd < NOFILE; fd++)
    //     p->ext4_ofile[fd] = NULL;
    
    // 关闭工作目录
    ext4_inode_put(p->ext4_cwd);
    p->ext4_cwd = NULL;
#endif

    spinlock_acquire(&parent_lock);

    // 让p的孩子认initproc作父
    proc_reparent(p);
    // 让p的父亲醒来收拾残局
    proc_wakeup(p->parent);
    
    // 获取p的锁以改变一些属性
    spinlock_acquire(&p->lk);
    p->exit_state = exit_state;
    p->state = ZOMBIE;

    spinlock_release(&parent_lock);

    proc_sched();

    panic("proc.c->proc_exit: 1\n");
}

/* 
    等待一个子进程退出 (若pid为-1可以是任意的,否则是指定的)
    在用户地址addr处填入子进程的exit_state
    成功则返回退出的子进程的pid, 失败则返回-1
*/
int proc_wait(int pid, uint64 addr)
{
    proc_t *child, *parent = myproc();
    bool havekids;
    int child_pid = -1;

    spinlock_acquire(&parent_lock); // 上锁-1

    while(1) {
        havekids = false;
        /* 判别阶段 */
        for(child = procs; child < procs + NPROC; child++) {
            // 遇到myproc的孩子 (step-1)
            if(child->parent == parent) {
                spinlock_acquire(&child->lk); // 上锁-2
                havekids = true;
                // 这个孩子是我们要找的 且 准备好退出了 (step-2)
                if((pid == -1 || child->pid == pid) && child->state == ZOMBIE) {
                    // 传递exit_state 销毁子进程并跳出循环 (step-3)
                    int exit_state = child->exit_state << 8;       // linux规定：高8位才是退出码
                    if(addr == 0 || uvm_copyout(parent->pagetable, addr, 
                            (uint64)(&exit_state), sizeof(exit_state)) == 0) {
                        child_pid = child->pid;
                        free_proc(child);
                        spinlock_release(&child->lk); // 解锁-2
                        break;
                    }
                }
                spinlock_release(&child->lk); // 解锁-2
            }
        }

        // case 1: 成功
        if(child_pid > 0) {
            spinlock_release(&parent_lock); // 解锁-1
            return child_pid;
        }
        // case 2: 失败
        if(!havekids || proc_iskilled(parent)) {
            spinlock_release(&parent_lock); // 解锁-1
            return -1;
        }
        // case 3: 等待proc_wakeup(parent)
        proc_sleep(parent, &parent_lock);
    }
}

/*
    进程状态变化: RUNNIGN->SLEEPING
    channel是进程休眠的地方,唤醒时参照channel选择要唤醒的进程
    注意：lock由调用者持有,保证proc只能在一个地方休眠
*/
void proc_sleep(void* channel, spinlock_t* lock)
{
    assert(channel != NULL, "proc.c->proc_sleep: 1\n");
    assert(spinlock_holding(lock), "proc.c->proc_sleep: 2\n");

    proc_t* p = myproc();

    spinlock_acquire(&p->lk);
    spinlock_release(lock);

    p->channel = channel;
    p->state = SLEEPING;
    proc_sched();
    // 唤醒时执行
    p->channel = NULL;

    spinlock_acquire(lock);
    spinlock_release(&p->lk);
}

/*
    进程状态变化: SLEEPING->RUNNABLE
    唤醒所有在channel上睡眠的进程
*/
void proc_wakeup(void* channel)
{
    // 遍历进程列表,找到符合条件的进程,唤醒
    proc_t* mp = myproc();
    for(proc_t* p = procs; p < &procs[NPROC]; p++) {
        if(p != mp) {
            spinlock_acquire(&p->lk);
            if(p->state == SLEEPING && p->channel == channel)
                p->state = RUNNABLE;
            spinlock_release(&p->lk);
        }
    }
}

/*
    将pid对应的进程的killed设为true (如果它睡眠则唤醒)
    成功返回0, 失败返回-1
*/
int proc_kill(int pid)
{
    for(proc_t* p = procs; p < procs + NPROC; p++) {
        spinlock_acquire(&p->lk);
        if(p->pid == pid) {           // 找到目标进程
            p->killed = true;         // 宣布该进程即将被kill
            if(p->state == SLEEPING)  // 唤醒它,使得它可以被调度
                p->state = RUNNABLE;
            spinlock_release(&p->lk);
            return 0;
        }
        spinlock_release(&p->lk);
    }
    return 0;
}

/*
    将进程p的所有子进程转让给initproc做孩子
    注意: 调用者应当持有parent_lock
*/
void proc_reparent(proc_t* p)
{
    for(proc_t* child = procs; child < procs + NPROC; child++) {
        if(child->parent == p) {
            child->parent = initproc;
            proc_wakeup(initproc);
        }
    }
}

/*
    return p->killed
*/
bool proc_iskilled(proc_t* p)
{
    spinlock_acquire(&p->lk);
    bool flag = p->killed;
    spinlock_release(&p->lk);
    return flag;
}

/*
    p->killed = true
*/
void proc_setkilled(proc_t* p)
{
    spinlock_acquire(&p->lk);
    p->killed = true;
    spinlock_release(&p->lk);
}

/*
    打印进程信息 for debug
*/
void proc_print(void)
{
  static char *states[] = {
    [UNUSED]    "unused\0",
    [USED]      "used\0",
    [SLEEPING]  "sleep\0",
    [RUNNABLE]  "runble\0",
    [RUNNING]   "running\0",
    [ZOMBIE]    "zombie\0"
  };
  printf("proc information:\n");
  for(proc_t* p = procs; p < &procs[NPROC]; p++) {
    if(p->state == UNUSED) continue;
    printf("pid = %d state = %s\n",p->pid,states[p->state]);
  }
}