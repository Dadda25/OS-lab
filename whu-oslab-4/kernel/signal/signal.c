#include "signal/signal.h"
#include "mem/vmem.h"
#include "mem/pmem.h"
#include "proc/cpu.h"
#include "lib/str.h"

// 成功返回0 失败返回-1
uint64 sig_action(int signum, uint64 addr_act, uint64 addr_oldact)
{
    proc_t* p = myproc();

    if(signum < 1 || signum > NSIG) return -1;
    if((addr_act != 0) && (signum == SIGKILL || signum == SIGSTOP)) return -1;

    spinlock_acquire(&p->lk);
    if(addr_oldact != 0) {
        if(uvm_copyout(p->pagetable, addr_oldact, 
        (uint64)(&p->sigactions[signum]), sizeof(sigaction_t)) < 0) {
            spinlock_release(&p->lk);
            return -1;   
        }
    }
    if(addr_act != 0) {
        if(uvm_copyin(p->pagetable, (uint64)(&p->sigactions[signum]), 
        addr_act, sizeof(sigaction_t)) < 0) {
            spinlock_release(&p->lk);
            return -1;
        }
    }
    spinlock_release(&p->lk);
    return 0;
}

// 成功返回0 失败返回-1
uint64 sig_procmask(int how, uint64 addr_set, uint64 addr_oldset)
{
    proc_t* p = myproc();
    sigset_t set;    
    
    spinlock_acquire(&p->lk);

    if(addr_oldset != 0) {
        if(uvm_copyout(p->pagetable, addr_oldset, 
        (uint64)(&p->sig_set), sizeof(p->sig_set)) < 0) {
            spinlock_release(&p->lk);
            return -1;
        }
    }

    if(addr_set != 0) {
        if(uvm_copyin(p->pagetable, (uint64)&set, 
        addr_set, sizeof(p->sig_set)) < 0) {
            spinlock_release(&p->lk);
            return -1;
        }
        for(int i = 0; i < SIGSET_LEN; i++) {
            switch (how) 
            {
                case SIG_BLOCK:
                    p->sig_set.val[i] |= set.val[i];
                    break;
                case SIG_UNBLOCK:
                    p->sig_set.val[i] &= ~(set.val[i]);
                    break;
                case SIG_SETMASK:
                    p->sig_set.val[i] = set.val[i];
                    break;                
                default:
                    break;
            }
        }
    }
    
    p->sig_set.val[0] &= 1ul << SIGTERM | 1ul << SIGKILL | 1 << SIGSTOP;
    spinlock_release(&p->lk);
    return 0;
}

uint64 sig_return()
{
    proc_t* p = myproc();
    memmove(p->tf, p->sig_frame, sizeof(struct trapframe));
    pmem_free_pages(p->sig_frame, 1, true);
    p->sig_frame = NULL;
    return p->tf->a0;
}

void sig_handle()
{
    
}
