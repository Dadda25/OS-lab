#include "proc/elf.h"
#include "proc/cpu.h"
#include "mem/vmem.h"
#include "fs/fat32_file.h"
#include "fs/fat32_inode.h"
#include "fs/fat32_dir.h"
#include "fs/ext4_file.h"
#include "fs/ext4_inode.h"
#include "fs/ext4_dir.h"
#include "fs/ext4_sys.h"
#include "fs/base_buf.h"
#include "lib/print.h"
#include "lib/str.h"

#define ADD_AUXV(id, val) \
    aux[index++] = id; \
    aux[index++] = val;

#ifdef FS_FAT32

static int loadseg(pgtbl_t pagetable, uint64 va, fat32_inode_t* ip, uint32 offset, uint32 sz)
{
    uint64 pa;
    int n, ret;
    for(int i = 0; i < sz; i += PAGE_SIZE) {
        pa = uvm_getpa(pagetable, va + i);
        assert(pa != 0, "exec.c->loadseg");
        n = min(sz - i, PAGE_SIZE);
        ret = fat32_inode_read(ip, offset + i, n, pa, false);
        if(ret != n) return -1;
    }
    return 0;
}

#else

static int loadseg(pgtbl_t pagetable, uint64 va, ext4_inode_t* ip, uint32 offset, uint32 sz)
{
    uint64 pa;
    int n, ret;
    assert(va % PAGE_SIZE == 0, "loadseg: 0");
    for(int i = 0; i < sz; i += PAGE_SIZE) {
        pa = uvm_getpa(pagetable, va + i);
        assert(pa != 0, "loadseg: 1");
        n = min(sz - i, PAGE_SIZE);
        ret = ext4_inode_read(ip, offset + i, n, (void*)pa, false);
        if(ret != n) return -1;
    }
    return 0;
}

#endif

static int flags_to_perm(int flags)
{
    int perm = 0;
    if(flags & 0x01)
        perm |= PTE_X;
    if(flags & 0x02)
        perm |= PTE_W;
    return perm;
}

// 返回需要的空间(page-aligned)
// 失败返回0
static uint64 get_total_mapping_size(elf_header_t *interpreter_elf, ext4_inode_t* interpreter) {
    uint64 min_addr = -1;
    uint64 max_addr = 0;
    bool is_true = false;
    program_header_t ph;
    for (uint32 i = 0, off = interpreter_elf->phoff; i < interpreter_elf->phnum; i++, off += sizeof(ph)) {
        assert(ext4_inode_read(interpreter, off, sizeof(ph), &ph, false) == sizeof(ph), "get_total_mapping_size");
        if (ph.type == ELF_PROG_LOAD) {
            min_addr = min(min_addr, ALIGN_DOWN(ph.vaddr, PAGE_SIZE));
            max_addr = max(max_addr, ph.vaddr + ph.memsz);
            is_true = true;
        }
    }
    return is_true ? (max_addr - min_addr) : 0;
}

static uint64 load_elf_interp(pgtbl_t pagetable, elf_header_t *interpreter_elf, ext4_inode_t* interpreter) {
    uint64 total_size = 0;
    uint64 start_addr = 0;
    program_header_t ph;
    /*----------------------获取总共需要的内存大小--------------------*/
    total_size = get_total_mapping_size(interpreter_elf, interpreter);
    if (total_size == 0) return -1;

    /*----------------------分配总共需要的内存大小--------------------*/
    start_addr = uvm_mmap(0, (int)total_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (start_addr == -1) return -1;

    /*----------------------加载elf文件--------------------*/
    for (uint32 i = 0, off = interpreter_elf->phoff; i < interpreter_elf->phnum; i++, off += sizeof(ph)) {
        ext4_inode_read(interpreter, off, sizeof(ph), &ph, false);

        if (ph.type == ELF_PROG_LOAD) {
            if (ph.memsz < ph.filesz) return -1;
            if (ph.vaddr + ph.memsz < ph.vaddr) return -1;

            uint32 margin_size = 0;
            if(ph.vaddr % PAGE_SIZE != 0)
                margin_size = ph.vaddr % PAGE_SIZE; 

            if(loadseg(pagetable, ALIGN_DOWN(ph.vaddr + start_addr, PAGE_SIZE), interpreter, ALIGN_DOWN(ph.off, PAGE_SIZE), ph.filesz + margin_size) < 0)
                return -1;
        }
    }
    return start_addr;
}

int proc_exec(char* path, char** argv, char** envp)
{
    int ret = 0;           // 函数返回值(int)
    uint64 uret = 0;       // 函数返回值(uint64)

    elf_header_t elf, interpreter_elf;    // 存放elf header
    program_header_t ph;                  // 存放programe_header
    pgtbl_t new_pgtbl = 0;                // 新的页表
    uint64 sz = 0, program_entry = 0;
    proc_t* p = myproc();
    bool is_dynamic = false;

#ifdef FS_FAT32
    // 根据path获得inode并上锁
    fat32_inode_t* ip = fat32_dir_searchPath(path, NULL);
    if(ip == NULL) {
        return -1;
    }
    fat32_inode_lock(ip);

    // 检查ELF header
    ret = fat32_inode_read(ip, 0, sizeof(elf), (uint64)&elf, false);
    if(ret != sizeof(elf)) goto bad;
    if(elf.magic != ELF_MAGIC) goto bad;
    
    // 申请一个新的页表 (trapframe和tramponline完成了映射)
    new_pgtbl = proc_alloc_pagetable(p);
    if(new_pgtbl == NULL) goto bad;

    // load program seg into memory
    for(int i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
        ret = fat32_inode_read(ip, off, sizeof(ph), (uint64)&ph, false);
        if(ret != sizeof(ph)) goto bad;
        if(ph.type != ELF_PROG_LOAD) continue;
        if(ph.memsz < ph.filesz) goto bad;
        if(ph.vaddr + ph.memsz < ph.vaddr) goto bad;
        if(ph.vaddr % PAGE_SIZE != 0) goto bad;

        uret = uvm_grow(new_pgtbl, sz, ph.vaddr + ph.memsz, flags_to_perm(ph.flags) | PTE_R);
        if(uret != ph.vaddr + ph.memsz) goto bad;
        sz = uret;

        ret = loadseg(new_pgtbl, ph.vaddr, ip, ph.off, ph.filesz);
        if(ret < 0) goto bad;
    }

    fat32_inode_unlockput(ip);
    ip = NULL;
#else
    // 为了实现重定向
    // for (int i = 1; i <= 4; i++) {
    //     printf("%s ", argv[i]);
    // }
    // printf("\n");
    if(argv[1] && argv[3] && argv[4] && strncmp(argv[1], "echo\0", 5) == 0) {
        if(strncmp(argv[3], ">\0", 2) == 0) { // 不追加
            ext4_file_close(p->ext4_ofile[1]); // 关闭标准输出
            p->ext4_ofile[1] = NULL;
            int fd = ext4_sys_openat(-100, argv[4], FLAGS_WRONLY | FLAGS_CREATE, 0);
            assert(fd == 1, "redirent: 0");
            argv[3] = NULL;
            argv[4] = NULL;
        } else if(strncmp(argv[3], ">>\0", 3) == 0) { // 追加
            ext4_file_close(p->ext4_ofile[1]); // 关闭标准输出
            p->ext4_ofile[1] = NULL;
            int fd = ext4_sys_openat(-100, argv[4], FLAGS_WRONLY | FLAGS_CREATE | FLAGS_APPEND, 0);
            assert(fd == 1, "redirent: 0");
            argv[3] = NULL;
            argv[4] = NULL;
        }
    }

    // 根据path获得inode并上锁
    ext4_inode_t* ip = ext4_dir_path_to_inode(path, NULL);
    if(ip == NULL) return -1;
    ext4_inode_lock(ip);

    // 检查ELF header
    ret = ext4_inode_read(ip, 0, sizeof(elf), &elf, false);
    if(ret != sizeof(elf)) goto bad;
    if(elf.magic != ELF_MAGIC) goto bad;
    
    // 申请一个新的页表 (trapframe和tramponline完成了映射)
    new_pgtbl = proc_alloc_pagetable(p);
    if(new_pgtbl == NULL) goto bad;

    // load program seg into memory
    program_header_t myph;
    bool getit = false;
    for(int i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
        ret = ext4_inode_read(ip, off, sizeof(ph), &ph, false);
        if(ret != sizeof(ph)) goto bad;
        if(ph.type == ELF_PROG_LOAD) {
            if(ph.memsz < ph.filesz) goto bad;
            if(ph.vaddr + ph.memsz < ph.vaddr) goto bad;
            if(!getit && ph.off <= elf.phoff && ph.off + ph.filesz > elf.phoff) {
                myph.vaddr = elf.phoff + ph.vaddr - ph.off;
                getit = true;
            }
            // if(ph.vaddr % PAGE_SIZE != 0) goto bad;

            uret = uvm_grow(new_pgtbl, sz, ph.vaddr + ph.memsz, flags_to_perm(ph.flags) | PTE_R);
            if(uret != ph.vaddr + ph.memsz) goto bad;
            sz = uret;

            uint32 margin_size = 0;
            if(ph.vaddr % PAGE_SIZE != 0)
                margin_size = ph.vaddr % PAGE_SIZE; 

            ret = loadseg(new_pgtbl, ALIGN_DOWN(ph.vaddr, PAGE_SIZE), ip, ALIGN_DOWN(ph.off, PAGE_SIZE), ph.filesz + margin_size);
            if(ret < 0) goto bad;
        } else if(ph.type == ELF_PROG_INTERP) {
            is_dynamic = true;
        }
    }
    ext4_inode_unlockput(ip);
    ip = NULL;
#endif

    pgtbl_t old_pgtbl = p->pagetable;
    uint64 oldsz = p->sz;
    vm_region_t* old_vm_head = p->vm_head;
    proc_destroy_pagetable(old_pgtbl, oldsz, old_vm_head);

/* -------------------动态链接处理----------------------*/
    // 由于动态链接要用到mmap 所以mmap相关初始化放在这边
    p->pagetable = new_pgtbl;
    p->vm_allocable = VM_MMAP_START;
    p->vm_head = NULL;
    uint64 start_addr = 0;

    is_dynamic = false;
    if(is_dynamic) {

        // 默认库函数是libc 找到inode并上锁
        ext4_inode_t* interpreter = ext4_dir_path_to_inode("/glibc/lib/libc.so", NULL);
        if(interpreter == NULL) panic("dlink: 0");        
        ext4_inode_lock(interpreter);

        // 读取ELF文件
        ret = ext4_inode_read(interpreter, 0, sizeof(interpreter_elf), &interpreter_elf, false);
        if(ret != sizeof(interpreter_elf)) panic("dlink: 1"); 
        if(interpreter_elf.magic != ELF_MAGIC) panic("dlink: 2");

        // 获取起始地址
        start_addr = load_elf_interp(p->pagetable, &interpreter_elf, interpreter);
        // printf("%p\n", start_addr);
        if (start_addr == -1) panic("dlink: 3");
        
        // 结束
        ext4_inode_unlockput(interpreter);
        program_entry = interpreter_elf.entry + start_addr;
    } else {
        program_entry = elf.entry;
    }

/*--------------------动态链接结束----------------------*/

    // 准备11个页面,低地址页面作为缓冲地带,高地址10个页面存放user-stack
    sz = ALIGN_UP(sz, PAGE_SIZE);
    uret = uvm_grow(new_pgtbl, sz, sz + 32 * PAGE_SIZE, PTE_W | PTE_R);
    if(uret == 0) goto bad;
    sz = uret;
    uvm_clear_PTEU(new_pgtbl, sz - 32 * PAGE_SIZE); // 缓冲页面在用户态不可访问

    // 填充参数到stack
    uint64 ustack[NARG], estack[NENV];
    int envc, argc;
    uint64 sp = sz; 
    uint64 stackbase = sp - PAGE_SIZE;

    // 随机数
    sp -= 16;
    uint64 random[2] = { 0x7be6f23c6eb43a7e, 0xb78b3ea1f7c8db96 };
    if(sp < stackbase || uvm_copyout(new_pgtbl, sp, (uint64)random, 16) < 0)
        goto bad;

    // aux准备
    uint64 aux[MAX_AT * 2];
    int index = 0;
    ADD_AUXV(AT_HWCAP, 0);
    ADD_AUXV(AT_PAGESZ, PAGE_SIZE);
    if(is_dynamic) {
        ADD_AUXV(AT_PHDR, myph.vaddr);
    } else {
        ADD_AUXV(AT_PHDR, elf.phoff);    
    }    
    ADD_AUXV(AT_PHENT, elf.phentsize);
    ADD_AUXV(AT_PHNUM, elf.phnum);
    ADD_AUXV(AT_BASE, start_addr);
    ADD_AUXV(AT_ENTRY, elf.entry);
    ADD_AUXV(AT_UID, 0);
    ADD_AUXV(AT_EUID, 0);
    ADD_AUXV(AT_GID, 0);
    ADD_AUXV(AT_EGID, 0);
    ADD_AUXV(AT_SECURE, 0);
    ADD_AUXV(AT_RANDOM, sp);
    ADD_AUXV(AT_NULL, 0);
   
    for(envc = 0; envp[envc]; envc++) {
        if(envc >= NENV) goto bad;
        sp -= strlen(envp[envc]) + 1;  // 腾出放参数的空间
        sp -= sp % 16;                 // riscv: sp must be 16 byte aligned
        if(sp < stackbase) goto bad;
        ret = uvm_copyout(new_pgtbl, sp, (uint64)envc[envp], strlen(envc[envp])+1);
        if(ret < 0) goto bad;
        estack[envc] = sp;
    }
    estack[envc] = 0;

    for(argc = 0; argv[argc] != NULL; argc++) {
        if(argc >= NARG) goto bad;
        sp -= strlen(argv[argc]) + 1;  // 腾出放参数的空间
        sp -= sp % 16;                 // riscv: sp must be 16 byte aligned
        if(sp < stackbase) goto bad;
        ret = uvm_copyout(new_pgtbl, sp, (uint64)argv[argc], strlen(argv[argc])+1);
        if(ret < 0) goto bad;
        ustack[argc] = sp;
    }
    ustack[argc] = 0;   

    // 填充aux到stack
    sp -= sizeof(aux);
    if(uvm_copyout(new_pgtbl, sp, (uint64)aux, sizeof(aux)) < 0)
        goto bad;

    // 填充环境变量指针到stack
    if(envp[0]) {
        sp -= (envc + 1) * sizeof(uint64);
        sp -= sp % 16;
        if(sp < stackbase) goto bad;
        ret = uvm_copyout(new_pgtbl, sp, (uint64)estack, (envc + 1) * sizeof(uint64));
        if(ret < 0) goto bad;
    }

    p->tf->a2 = sp;

    // 填充参数指针到stack
    sp -= (argc + 1) * sizeof(uint64);
    sp -= sp % 16;
    if(sp < stackbase) goto bad;
    ret = uvm_copyout(new_pgtbl, sp, (uint64)ustack, (argc + 1) * sizeof(uint64));
    if(ret < 0) goto bad;
    sp -= sizeof(uint64);
    ret = uvm_copyout(new_pgtbl, sp, (uint64)&argc, sizeof(uint64));
    if(ret < 0) goto bad;
    
    p->tf->a1 = sp;

    p->sz = sz;
    p->tf->epc = program_entry;
    p->tf->sp = sp;

    return argc;

bad:
    if(new_pgtbl) proc_destroy_pagetable(new_pgtbl, sz, NULL);

#ifdef FS_FAT32
        if(ip) 
            fat32_inode_unlockput(ip);
#else
        if(ip) 
            ext4_inode_unlockput(ip);
#endif    
// 0x0000-0000-1CE0-CD54
    return -1;
}