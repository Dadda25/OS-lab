// 第一个进程的内容
// 针对比赛要求逐个运行测试用例
// 这个文件参考了全年的二等奖作品: AVX 来自华中科技大学
// https://gitlab.eduxiji.net/202310487101114/oskernel2023-avx

#include "include/sys.h"

typedef struct {
  int valid;
  char *name[10];
} testcase;

static char* envp[] = {
    0
};



static char* basic_glibc_names[];
static char* basic_musl_names[];
static char* busybox_glibc_names[];
static char* busybox_musl_names[];



static testcase basic_glibc[];
static testcase basic_musl[];
static testcase busybox_glibc[];
static testcase busybox_musl[];
// static testcase lmbench_glibc[];
static testcase lua_glibc[];
static testcase libctest_static[];
static testcase libctest_dynamic[];


inline static int get_len(char* s) {
    int i;
    for(i = 0; s[i] != '\0'; i++);
    return i;
}

int main()
{   
    int pid = 0;
    int ret = 0;
    int status = 0;


    
    // basic-glibc

    syscall(SYS_write, 1, "#### OS COMP TEST GROUP START basic-glibc ####\n", 48);
    for (int i = 0; basic_glibc[i].name[0]; i++) {
        syscall(SYS_write, 1, "Testing ", 9);
        syscall(SYS_write, 1, basic_glibc_names[i], get_len(basic_glibc_names[i]));
        syscall(SYS_write, 1, " :\n", 4);
        pid = syscall(SYS_clone, 17, 0);
        if(pid < 0) {
            syscall(SYS_write, 1, "fork failed\n", 13);
            syscall(SYS_exit, 0);
        } else if(pid == 0) {
            syscall(SYS_chdir,"/glibc/basic/");
            ret = syscall(SYS_execve, basic_glibc[i].name[0], basic_glibc[i].name, envp);
            syscall(SYS_exit, 0);
        } else {
            syscall(SYS_wait4, pid, &status, 0, 0);
        }    
    }
    syscall(SYS_write, 1, "#### OS COMP TEST GROUP END basic-glibc ####\n", 46);
    


    // basic-musl

    syscall(SYS_write, 1, "#### OS COMP TEST GROUP START basic-musl ####\n", 47);
    for(int i = 0; basic_musl[i].name[0]; i++) {
        syscall(SYS_write, 1, "Testing ", 9);
        syscall(SYS_write, 1, basic_musl_names[i], get_len(basic_musl_names[i]));
        syscall(SYS_write, 1, " :\n", 4);
        pid = syscall(SYS_clone, 17, 0);
        if(pid < 0) {
            syscall(SYS_write, 1, "fork failed\n", 13);
            syscall(SYS_exit, 0);
        } else if(pid == 0) {
            syscall(SYS_chdir,"/musl/basic/");
            ret = syscall(SYS_execve, basic_musl[i].name[0], basic_musl[i].name, envp);
            syscall(SYS_exit, 0);
        } else {
            syscall(SYS_wait4, pid, &status, 0, 0);
        }    
    }
    syscall(SYS_write, 1, "#### OS COMP TEST GROUP END basic-musl ####\n", 45);


    
    // busybox-glibc
    syscall(SYS_write, 1, "#### OS COMP TEST GROUP START busybox-glibc ####\n", 50);
    for (int i = 0; busybox_glibc[i].name[0]; i++) {
        pid = syscall(SYS_clone, 17, 0);
        if(pid < 0) {
            syscall(SYS_write, 1, "fork failed\n", 13);
            syscall(SYS_exit, 0);
        } else if(pid == 0) {
            syscall(SYS_chdir, "/glibc");
            syscall(SYS_execve,"/musl/busybox" , busybox_glibc[i].name, envp);
            syscall(SYS_exit, 0);
        } else {
            syscall(SYS_wait4, pid, &status, 0, 0);
            syscall(SYS_write, 1, "testcase busybox ", 18);
            syscall(SYS_write, 1, busybox_glibc_names[i], get_len(busybox_glibc_names[i]));
            syscall(SYS_write, 1, " success\n", 10);
        }
    }
    syscall(SYS_write, 1, "#### OS COMP TEST GROUP END busybox-glibc ####\n", 48);

    

    // busybox-musl
    syscall(SYS_write, 1, "#### OS COMP TEST GROUP START busybox-musl ####\n", 49);
    for (int i = 0; busybox_musl[i].name[0]; i++) {
        pid = syscall(SYS_clone, 17, 0);
        if(pid < 0) {
            syscall(SYS_write, 1, "fork failed\n", 13);
            syscall(SYS_exit, 0);
        } else if(pid == 0) {
            syscall(SYS_chdir, "/musl");
            syscall(SYS_execve,"/musl/busybox" , busybox_musl[i].name, envp);
            syscall(SYS_exit, 0);
        } else {
            syscall(SYS_wait4, pid, &status, 0, 0);
            syscall(SYS_write, 1, "testcase busybox ", 18);
            syscall(SYS_write, 1, busybox_musl_names[i], get_len(busybox_musl_names[i]));
            syscall(SYS_write, 1, " success\n", 10);
        }
    }
    syscall(SYS_write, 1, "#### OS COMP TEST GROUP END busybox-musl ####\n", 47);



    // lua-glibc
    syscall(SYS_write, 1, "#### OS COMP TEST GROUP START lua-glibc ####\n", 46);
    for (int i = 0; lua_glibc[i].name[0]; i++) {
        pid = syscall(SYS_clone, 17, 0);
        if(pid < 0) {
            syscall(SYS_write, 1, "fork failed\n", 13);
            syscall(SYS_exit, 0);
        } else if(pid == 0) {
            syscall(SYS_chdir,"/glibc");
            ret = syscall(SYS_execve, "/musl/lua", lua_glibc[i].name, envp);
            syscall(SYS_exit, 0);
        } else {
            syscall(SYS_wait4, pid, &status, 0, 0);
            syscall(SYS_write, 1, "testcase lua ", 14);
            syscall(SYS_write, 1, lua_glibc[i].name[1], get_len(lua_glibc[i].name[1]));
            if (ret == 0) {
                syscall(SYS_write, 1, " success\n", 10);
            } else {
                syscall(SYS_write, 1, " failed\n", 9);
            }
        }
    }
    syscall(SYS_write, 1, "#### OS COMP TEST GROUP END lua-glibc ####\n", 44);
    
    
    
    // lua-musl
    syscall(SYS_write, 1, "#### OS COMP TEST GROUP START lua-musl ####\n", 46);
    for (int i = 0; lua_glibc[i].name[0]; i++) {
        pid = syscall(SYS_clone, 17, 0);
        if(pid < 0) {
            syscall(SYS_write, 1, "fork failed\n", 13);
            syscall(SYS_exit, 0);
        } else if(pid == 0) {
            syscall(SYS_chdir,"/musl");
            ret = syscall(SYS_execve, "/musl/lua", lua_glibc[i].name, envp);
            syscall(SYS_exit, 0);
        } else {
            syscall(SYS_wait4, pid, &status, 0, 0);
            syscall(SYS_write, 1, "testcase lua ", 14);
            syscall(SYS_write, 1, lua_glibc[i].name[1], get_len(lua_glibc[i].name[1]));
            if (ret == 0) {
                syscall(SYS_write, 1, " success\n", 10);
            } else {
                syscall(SYS_write, 1, " failed\n", 9);
            }
        }
    }
    syscall(SYS_write, 1, "#### OS COMP TEST GROUP END lua-musl ####\n", 44);
    
    

    
    // libc-test-musl
    syscall(SYS_write, 1, "#### OS COMP TEST GROUP START libctest-musl ####\n", 50);
    for(int i = 0; libctest_static[i].name[1]; i++) {
        if(libctest_static[i].valid == 0) continue;
        pid = syscall(SYS_clone, 17, 0);
        if(pid < 0) {
            syscall(SYS_write, 1, "fork failed\n", 13);
            syscall(SYS_exit, 0);
        } else if(pid == 0) {
            syscall(SYS_chdir,"/musl");
            syscall(SYS_execve, "./runtest.exe", libctest_static[i].name, envp);
            syscall(SYS_exit, 0);
        } else {
            syscall(SYS_wait4, pid, &status, 0, 0);
        }      
    }
    for(int i = 0; libctest_dynamic[i].name[1]; i++) {
        if(libctest_dynamic[i].valid == 0) continue;
        pid = syscall(SYS_clone, 17, 0);
        if(pid < 0) {
            syscall(SYS_write, 1, "fork failed\n", 13);
            syscall(SYS_exit, 0);
        } else if(pid == 0) {
            syscall(SYS_chdir,"/musl");
            syscall(SYS_execve, "./runtest.exe", libctest_dynamic[i].name, envp);
            syscall(SYS_exit, 0);
        } else {
            syscall(SYS_wait4, -1, 0);
        }      
    }
    syscall(SYS_write, 1, "#### OS COMP TEST GROUP END libctest-musl ####\n", 48);
    

    // ltp
    // syscall(SYS_write, 1, "#### OS COMP TEST GROUP START ltp-glibc ####\n", 46);
    // for(int i = 0; ltp_glibc[i].name[1]; i++) {
    //     if(ltp_glibc[i].valid == 0) continue;
    //     pid = syscall(SYS_clone, 17, 0);
    //     if(pid < 0) {
    //         syscall(SYS_write, 1, "fork failed\n", 13);
    //         syscall(SYS_exit, 0);
    //     } else if(pid == 0) {
    //         syscall(SYS_chdir,"/musl");
    //         syscall(SYS_execve, "./runtest.exe", ltp_glibc[i].name, envp);
    //         syscall(SYS_exit, 0);
    //     } else {
    //         syscall(SYS_wait4, -1, 0);
    //     }      
    // }
    // syscall(SYS_write, 1, "#### OS COMP TEST GROUP END ltp-glibc ####\n", 44);
    

    syscall(SYS_shutdown);    
    return 0;
}



static char* basic_glibc_names[] = {
    "brk",
    "chdir",
    "clone",
    "close",
    "dup2",
    "dup",
    "execve",
    "exit",
    "fork",
    "fstat",
    "getcwd",
    "getdents",
    "getpid",
    "getppid",
    "gettimeofday",
    "mkdir_",
    "mmap",
    "mount",
    "munmap",
    "openat",
    "open",
    "pipe",
    "read",
    "sleep",
    "times",
    "umount",
    "uname",
    "unlink",
    "wait",
    "waitpid",
    "write",
    "yield",
    0
};

static char* basic_musl_names[] = {
    "brk",
    "chdir",
    "clone",
    "close",
    "dup2",
    "dup",
    "execve",
    "exit",
    "fork",
    "fstat",
    "getcwd",
    "getdents",
    "getpid",
    "getppid",
    "gettimeofday",
    "mkdir_",
    "mmap",
    "mount",
    "munmap",
    "openat",
    "open",
    "pipe",
    "read",
    "sleep",
    "times",
    "umount",
    "uname",
    "unlink",
    "wait",
    "waitpid",
    "write",
    "yield"
};

static testcase basic_glibc[] = {
    {1, {"/glibc/basic/brk", 0}},
    {1, {"/glibc/basic/chdir", 0}},
    {1, {"/glibc/basic/clone", 0}},
    {1, {"/glibc/basic/close", 0}},
    {1, {"/glibc/basic/dup2", 0}},
    {1, {"/glibc/basic/dup", 0}},
    {1, {"/glibc/basic/execve", 0}},
    {1, {"/glibc/basic/exit", 0}},
    {1, {"/glibc/basic/fork", 0}},
    {1, {"/glibc/basic/fstat", 0}},
    {1, {"/glibc/basic/getcwd", 0}},
    {1, {"/glibc/basic/getdents", 0}},
    {1, {"/glibc/basic/getpid", 0}},
    {1, {"/glibc/basic/getppid", 0}},
    {1, {"/glibc/basic/gettimeofday", 0}},
    {1, {"/glibc/basic/mkdir_", 0}},
    {1, {"/glibc/basic/mmap", 0}},
    {1, {"/glibc/basic/mount", 0}},
    {1, {"/glibc/basic/munmap", 0}},
    {1, {"/glibc/basic/openat", 0}},
    {1, {"/glibc/basic/open", 0}},
    {1, {"/glibc/basic/pipe", 0}},
    {1, {"/glibc/basic/read", 0}},
    {1, {"/glibc/basic/sleep", 0}},
    {1, {"/glibc/basic/times", 0}},
    {1, {"/glibc/basic/umount", 0}},
    {1, {"/glibc/basic/uname", 0}},
    {1, {"/glibc/basic/unlink", 0}},
    {1, {"/glibc/basic/wait", 0}},
    {1, {"/glibc/basic/waitpid", 0}},
    {1, {"/glibc/basic/write", 0}},
    {1, {"/glibc/basic/yield", 0}},
    {1, {0, 0}}
};

static testcase basic_musl[] = {
    {1, {"/musl/basic/brk", 0}},
    {1, {"/musl/basic/chdir", 0}},
    {1, {"/musl/basic/clone", 0}},
    {1, {"/musl/basic/close", 0}},
    {1, {"/musl/basic/dup2", 0}},
    {1, {"/musl/basic/dup", 0}},
    {1, {"/musl/basic/execve", 0}},
    {1, {"/musl/basic/exit", 0}},
    {1, {"/musl/basic/fork", 0}},
    {1, {"/musl/basic/fstat", 0}},
    {1, {"/musl/basic/getcwd", 0}},
    {1, {"/musl/basic/getdents", 0}},
    {1, {"/musl/basic/getpid", 0}},
    {1, {"/musl/basic/getppid", 0}},
    {1, {"/musl/basic/gettimeofday", 0}},
    {1, {"/musl/basic/mkdir_", 0}},
    {1, {"/musl/basic/mmap", 0}},
    {1, {"/musl/basic/mount", 0}},
    {1, {"/musl/basic/munmap", 0}},
    {1, {"/musl/basic/openat", 0}},
    {1, {"/musl/basic/open", 0}},
    {1, {"/musl/basic/pipe", 0}},
    {1, {"/musl/basic/read", 0}},
    {1, {"/musl/basic/sleep", 0}},
    {1, {"/musl/basic/times", 0}},
    {1, {"/musl/basic/umount", 0}},
    {1, {"/musl/basic/uname", 0}},
    {1, {"/musl/basic/unlink", 0}},
    {1, {"/musl/basic/wait", 0}},
    {1, {"/musl/basic/waitpid", 0}},
    {1, {"/musl/basic/write", 0}},
    {1, {"/musl/basic/yield", 0}},
    {1, {0, 0}}
};



static char* busybox_glibc_names[] = {
    "echo \"#### independent command test\"",
    "ash -c exit",
    "sh -c exit",
    "basename /aaa/bbb",
    "cal",
    "clear",
    "date", 
    "df",
    "dirname /aaa/bbb",
    "dmesg",
    "du",
    "expr 1 + 1",
    "false",
    "true",
    "which ls",
    "uname",
    "uptime",
    "printf \"abc\\n\"",
    "ps",
    "pwd",
    "free",
    "hwclock",
    "sh -c \'sleep 5\' & ./busybox kill $!",
    "ls",
    "sleep 1",
    "echo \"#### file opration test\"",
    "touch test.txt",
    "echo \"hello world\" > test.txt",
    "cat test.txt",
    "cut -c 3 test.txt",
    "od test.txt",
    "head test.txt",
    "tail test.txt",
    "hexdump -C test.txt", 
    "md5sum test.txt",
    "echo \"ccccccc\" >> test.txt",
    "echo \"bbbbbbb\" >> test.txt",
    "echo \"aaaaaaa\" >> test.txt",
    "echo \"2222222\" >> test.txt",
    "echo \"1111111\" >> test.txt",
    "echo \"bbbbbbb\" >> test.txt",
    "sort test.txt | ./busybox uniq",
    "stat test.txt",
    "strings test.txt", 
    "wc test.txt",
    "[ -f test.txt ]",
    "more test.txt",
    "rm test.txt",
    "mkdir test_dir",
    "mv test_dir test",
    "rmdir test",
    "grep hello busybox_cmd.txt",
    "cp busybox_cmd.txt busybox_cmd.bak",
    "rm busybox_cmd.bak",
    "find -name \"busybox_cmd.txt\"",
};

static char* busybox_musl_names[] = {
    "echo \"#### independent command test\"",
    "ash -c exit",
    "sh -c exit",
    "basename /aaa/bbb",
    "cal",
    "clear",
    "date", 
    "df",
    "dirname /aaa/bbb",
    "dmesg",
    "du",
    "expr 1 + 1",
    "false",
    "true",
    "which ls",
    "uname",
    "uptime",
    "printf \"abc\\n\"",
    "ps",
    "pwd",
    "free",
    "hwclock",
    "sh -c \'sleep 5\' & ./busybox kill $!",
    "ls",
    "sleep 1",
    "echo \"#### file opration test\"",
    "touch test.txt",
    "echo \"hello world\" > test.txt",
    "cat test.txt",
    "cut -c 3 test.txt",
    "od test.txt",
    "head test.txt",
    "tail test.txt",
    "hexdump -C test.txt", 
    "md5sum test.txt",
    "echo \"ccccccc\" >> test.txt",
    "echo \"bbbbbbb\" >> test.txt",
    "echo \"aaaaaaa\" >> test.txt",
    "echo \"2222222\" >> test.txt",
    "echo \"1111111\" >> test.txt",
    "echo \"bbbbbbb\" >> test.txt",
    "sort test.txt | ./busybox uniq",
    "stat test.txt",
    "strings test.txt", 
    "wc test.txt",
    "[ -f test.txt ]",
    "more test.txt",
    "rm test.txt",
    "mkdir test_dir",
    "mv test_dir test",
    "rmdir test",
    "grep hello busybox_cmd.txt",
    "cp busybox_cmd.txt busybox_cmd.bak",
    "rm busybox_cmd.bak",
    "find -name \"busybox_cmd.txt\"",
};

static testcase busybox_glibc[] = {
    {1, {"/musl/busybox", "echo", "#### independent command test", 0}},
    {1, {"/musl/busybox", "ash", "-c", "exit", 0}},
    {1, {"/musl/busybox", "sh", "-c", "exit", 0}},
    {1, {"/musl/busybox", "basename", "/aaa/bbb", 0}},
    {1, {"/musl/busybox", "cal", 0}},
    // {1, {"/musl/busybox", "clear", 0}},
    {1, {"/musl/busybox", "date", 0}},
    {1, {"/musl/busybox", "df", 0}},
    {1, {"/musl/busybox", "dirname", "/aaa/bbb", 0}},
    {1, {"/musl/busybox", "dmesg", 0}},
    {1, {"/musl/busybox", "du", 0}},
    {1, {"/musl/busybox", "expr", "1", "+", "1", 0}},
    {1, {"/musl/busybox", "false", 0}},
    {1, {"/musl/busybox", "true", 0}},
    {1, {"/musl/busybox", "which", "ls", 0}},
    {1, {"/musl/busybox", "uname", 0}},
    {1, {"/musl/busybox", "uptime", 0}},
    {1, {"/musl/busybox", "printf", "abc\n", 0}},
    {1, {"/musl/busybox", "ps", 0}},
    {1, {"/musl/busybox", "pwd", 0}},
    {1, {"/musl/busybox", "free", 0}},
    {1, {"/musl/busybox", "hwclock", 0}},
    {1, {"/musl/busybox", "sh", "-c", "\'sleep 5\'", "&", "./busybox", "kill", "$!", 0}},
    {1, {"/musl/busybox", "ls", 0}},
    {1, {"/musl/busybox", "sleep", "1", 0}},
    {1, {"/musl/busybox", "echo", "#### file opration test", 0}},
    {1, {"/musl/busybox", "touch", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "hello world", ">", "test.txt", 0}},
    {1, {"/musl/busybox", "cat", "test.txt", 0}},
    {1, {"/musl/busybox", "cut", "-c", "3", "test.txt", 0}},
    {1, {"/musl/busybox", "od", "test.txt", 0}},
    {1, {"/musl/busybox", "head", "test.txt", 0}},
    {1, {"/musl/busybox", "tail", "test.txt", 0}},
    {1, {"/musl/busybox", "hexdump", "-C", "test.txt", 0}},
    {1, {"/musl/busybox", "md5sum", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "ccccccc", ">>", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "bbbbbbb", ">>", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "aaaaaaa", ">>", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "2222222", ">>", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "1111111", ">>", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "bbbbbbb", ">>", "test.txt", 0}},
    {1, {"/musl/busybox", "sort", "test.txt", "|", "./busybox", "uniq", 0}},
    {1, {"/musl/busybox", "stat", "test.txt", 0}},
    {1, {"/musl/busybox", "strings", "test.txt", 0}},
    {1, {"/musl/busybox", "wc", "test.txt", 0}},
    {1, {"/musl/busybox", "[", "-f", "test.txt", "]", 0}},
    {1, {"/musl/busybox", "more", "test.txt", 0}},
    {1, {"/musl/busybox", "rm", "test.txt", 0}},
    {1, {"/musl/busybox", "mkdir", "test_dir", 0}},
    {1, {"/musl/busybox", "mv", "test_dir", "test", 0}},
    {1, {"/musl/busybox", "rmdir", "test", 0}},
    {1, {"/musl/busybox", "grep", "hello", "busybox_cmd.txt", 0}},
    {1, {"/musl/busybox", "cp", "busybox_cmd.txt", "busybox_cmd.bak", 0}},
    {1, {"/musl/busybox", "rm", "busybox_cmd.bak", 0}},
    {1, {"/musl/busybox", "find", "-name", "busybox_cmd.txt", 0}},
    {1, {0, 0}},
};

static testcase busybox_musl[] = {
    {1, {"/musl/busybox", "echo", "#### independent command test", 0}},
    {1, {"/musl/busybox", "ash", "-c", "exit", 0}},
    {1, {"/musl/busybox", "sh", "-c", "exit", 0}},
    {1, {"/musl/busybox", "basename", "/aaa/bbb", 0}},
    {1, {"/musl/busybox", "cal", 0}},
    // {1, {"/musl/busybox", "clear", 0}},
    {1, {"/musl/busybox", "date", 0}},
    {1, {"/musl/busybox", "df", 0}},
    {1, {"/musl/busybox", "dirname", "/aaa/bbb", 0}},
    {1, {"/musl/busybox", "dmesg", 0}},
    {1, {"/musl/busybox", "du", 0}},
    {1, {"/musl/busybox", "expr", "1", "+", "1", 0}},
    {1, {"/musl/busybox", "false", 0}},
    {1, {"/musl/busybox", "true", 0}},
    {1, {"/musl/busybox", "which", "ls", 0}},
    {1, {"/musl/busybox", "uname", 0}},
    {1, {"/musl/busybox", "uptime", 0}},
    {1, {"/musl/busybox", "printf", "abc\n", 0}},
    {1, {"/musl/busybox", "ps", 0}},
    {1, {"/musl/busybox", "pwd", 0}},
    {1, {"/musl/busybox", "free", 0}},
    {1, {"/musl/busybox", "hwclock", 0}},
    {1, {"/musl/busybox", "sh", "-c", "\'sleep 5\'", "&", "./busybox", "kill", "$!", 0}},
    {1, {"/musl/busybox", "ls", 0}},
    {1, {"/musl/busybox", "sleep", "1", 0}},
    {1, {"/musl/busybox", "echo", "#### file opration test", 0}},
    {1, {"/musl/busybox", "touch", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "hello world", ">", "test.txt", 0}},
    {1, {"/musl/busybox", "cat", "test.txt", 0}},
    {1, {"/musl/busybox", "cut", "-c", "3", "test.txt", 0}},
    {1, {"/musl/busybox", "od", "test.txt", 0}},
    {1, {"/musl/busybox", "head", "test.txt", 0}},
    {1, {"/musl/busybox", "tail", "test.txt", 0}},
    {1, {"/musl/busybox", "hexdump", "-C", "test.txt", 0}},
    {1, {"/musl/busybox", "md5sum", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "ccccccc", ">>", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "bbbbbbb", ">>", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "aaaaaaa", ">>", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "2222222", ">>", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "1111111", ">>", "test.txt", 0}},
    {1, {"/musl/busybox", "echo", "bbbbbbb", ">>", "test.txt", 0}},
    {1, {"/musl/busybox", "sort", "test.txt", "|", "./busybox", "uniq", 0}},
    {1, {"/musl/busybox", "stat", "test.txt", 0}},
    {1, {"/musl/busybox", "strings", "test.txt", 0}},
    {1, {"/musl/busybox", "wc", "test.txt", 0}},
    {1, {"/musl/busybox", "[", "-f", "test.txt", "]", 0}},
    {1, {"/musl/busybox", "more", "test.txt", 0}},
    {1, {"/musl/busybox", "rm", "test.txt", 0}},
    {1, {"/musl/busybox", "mkdir", "test_dir", 0}},
    {1, {"/musl/busybox", "mv", "test_dir", "test", 0}},
    {1, {"/musl/busybox", "rmdir", "test", 0}},
    {1, {"/musl/busybox", "grep", "hello", "busybox_cmd.txt", 0}},
    {1, {"/musl/busybox", "cp", "busybox_cmd.txt", "busybox_cmd.bak", 0}},
    {1, {"/musl/busybox", "rm", "busybox_cmd.bak", 0}},
    {1, {"/musl/busybox", "find", "-name", "busybox_cmd.txt", 0}},
    {1, {0, 0}},
};




// static testcase lmbench_glibc[] = {
//     {1, {"/musl/lmbench_all", "lat_syscall", "-P", "1", "null", 0}},
//     {1, {0, 0}},
// };


static testcase libctest_static[] = {
    {1, {"./runtest.exe", "-w", "entry-static.exe", "argv", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "basename", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "clocale_mbfuncs", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "clock_gettime", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "dirname", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "env", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "fdopen", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fnmatch", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "fscanf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fwscanf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "iconv_open", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "inet_pton", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "mbc", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "memstream", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel_points", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cond", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_tsd", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "qsort", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "random", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "search_hsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "search_insque", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "search_lsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "search_tsearch", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "setjmp", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "snprintf", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "socket", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "sscanf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "sscanf_long", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "stat", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strftime", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_memcpy", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_memmem", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_memset", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_strchr", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_strcspn", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "string_strstr", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strptime", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtod", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtod_simple", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtof", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtol", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strtold", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "swprintf", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "tgmath", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "time", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "tls_align", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "udiv", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "ungetc", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "utime", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "wcsstr", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "wcstol", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "daemon_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "dn_expand_empty", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "dn_expand_ptr_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fflush_exit", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fgets_eof", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "fgetwc_buffering", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "fpclassify_invalid_ld80", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "ftello_unflushed_append", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "getpwnam_r_crash", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "getpwnam_r_errno", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "iconv_roundtrips", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "inet_ntop_v4mapped", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "inet_pton_empty_last_field", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "iswspace_null", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "lrand48_signextend", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "lseek_large", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "malloc_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "mbsrtowcs_overflow", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "memmem_oob_read", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "memmem_oob", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "mkdtemp_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "mkstemp_failure", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "printf_1e9_oob", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "printf_fmt_g_round", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "printf_fmt_g_zeros", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "printf_fmt_n", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_robust_detach", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cancel_sem_wait", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_cond_smasher", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_condattr_setclock", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_exit_cancel", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_once_deadlock", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "pthread_rwlock_ebusy", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "putenv_doublefree", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_backref_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_bracket_icase", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_ere_backref", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_escaped_high_byte", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regex_negated_range", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "regexec_nosub", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "rewind_clear_error", 0}},
    // {1, {"./runtest.exe", "-w", "entry-static.exe", "rlimit_open_files", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "scanf_bytes_consumed", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "scanf_match_literal_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "scanf_nullbyte_char", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "setvbuf_unget", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "sigprocmask_internal", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "sscanf_eof", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "statvfs", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "strverscmp", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "syscall_sign_extend", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "uselocale_0", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "wcsncpy_read_overflow", 0}},
    {1, {"./runtest.exe", "-w", "entry-static.exe", "wcsstr_false_negative", 0}},
    {1, {0, 0}}, // 数组结束标志，必须保留
};

static testcase libctest_dynamic[] = {
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "argv", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "basename", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "clocale_mbfuncs", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "clock_gettime", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "dirname", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "env", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fdopen", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fnmatch", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fscanf", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fwscanf", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "iconv_open", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "inet_pton", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "mbc", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "memstream", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cancel_points", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cancel", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cond", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_tsd", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "qsort", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "random", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "search_hsearch", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "search_insque", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "search_lsearch", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "search_tsearch", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "setjmp", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "snprintf", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "socket", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sscanf", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sscanf_long", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "stat", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strftime", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_memcpy", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_memmem", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_memset", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_strchr", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_strcspn", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "string_strstr", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strptime", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtod", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtod_simple", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtof", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtol", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strtold", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "swprintf", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "tgmath", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "time", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "tls_align", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "udiv", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "ungetc", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "utime", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "wcsstr", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "wcstol", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "daemon_failure", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "dn_expand_empty", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "dn_expand_ptr_0", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fflush_exit", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fgets_eof", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fgetwc_buffering", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "fpclassify_invalid_ld80", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "ftello_unflushed_append", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "getpwnam_r_crash", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "getpwnam_r_errno", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "iconv_roundtrips", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "inet_ntop_v4mapped", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "inet_pton_empty_last_field", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "iswspace_null", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "lrand48_signextend", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "lseek_large", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "malloc_0", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "mbsrtowcs_overflow", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "memmem_oob_read", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "memmem_oob", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "mkdtemp_failure", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "mkstemp_failure", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "printf_1e9_oob", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "printf_fmt_g_round", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "printf_fmt_g_zeros", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "printf_fmt_n", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_robust_detach", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cancel_sem_wait", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_cond_smasher", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_condattr_setclock", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_exit_cancel", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_once_deadlock", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "pthread_rwlock_ebusy", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "putenv_doublefree", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_backref_0", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_bracket_icase", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_ere_backref", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_escaped_high_byte", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regex_negated_range", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "regexec_nosub", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "rewind_clear_error", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "rlimit_open_files", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "scanf_bytes_consumed", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "scanf_match_literal_eof", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "scanf_nullbyte_char", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "setvbuf_unget", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sigprocmask_internal", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "sscanf_eof", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "statvfs", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "strverscmp", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "syscall_sign_extend", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "uselocale_0", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "wcsncpy_read_overflow", 0}},
    // {1, {"./runtest.exe", "-w", "entry-dynamic.exe", "wcsstr_false_negative", 0}},
    {1, {0, 0}}, // 数组结束标志，必须保留
};




static testcase lua_glibc[] = {
    {1, {"./lua", "date.lua", 0}},    
    {1, {"./lua", "file_io.lua", 0}},
    {1, {"./lua", "max_min.lua", 0}}, 
    {1, {"./lua", "random.lua", 0}},
    {1, {"./lua", "remove.lua", 0}},  
    {1, {"./lua", "round_num.lua", 0}},
    {1, {"./lua", "sin30.lua", 0}},   
    {1, {"./lua", "sort.lua", 0}},
    {1, {"./lua", "strings.lua", 0}}, 
    {1, {0}},
};


