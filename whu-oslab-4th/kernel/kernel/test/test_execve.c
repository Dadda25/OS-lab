#include "common.h"

bool check_execve_valid(char* path, char** argv) {
    // 检查路径是否为空
    if (path == NULL || path[0] == '\0') {
        return -1;
    }
    
    // 检查路径长度是否合理
    int path_len = 0;
    while (path[path_len] != '\0' && path_len < PATH_LEN) {
        path_len++;
    }
    if (path_len >= PATH_LEN) {
        return -1;
    }
    
    // 检查argv是否为空
    if (argv == NULL) {
        return -1;
    }
    
    // 检查argv[0]是否存在（程序名）
    if (argv[0] == NULL) {
        return -1;
    }
    
    // 计算参数个数，防止参数过多
    int argc = 0;
    while (argv[argc] != NULL && argc < NARG) {
        // 检查每个参数的长度是否合理
        int arg_len = 0;
        while (argv[argc][arg_len] != '\0' && arg_len < ARG_LEN) {
            arg_len++;
        }
        if (arg_len >= ARG_LEN) {
            return -1;
        }
        argc++;
    }
    
    // 检查参数是否过多
    if (argc >= NARG && argv[argc] != NULL) {
        return -1;
    }
    
    return 0;  // 验证通过
}