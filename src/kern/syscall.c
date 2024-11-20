/**
 * syscall.c - kernel-level syscalls for user programs
 */

#include "../user/scnum.h"
#include "../user/syscall.h"
#include "console.h"
#include "memory.h"
#include "process.h"
#include "fs.h"
#include "excp.c"

static int sysexit(void){
    process_exit();
    return 0;
}

static int sysmsgout(const char *msg){
    
    return 0;
}

static int sysdevopen(int fd, const char *name, int instno){
    
    return ;
}

static int sysfsopen(int fd, const char *name){

    return ;
}

static int sysclose(int fd){
    return ;
}

static long sysread(int fd, void *buf, size_t bufsz){

}

static long syswrite(int fd, const void *buf, size_t len){

}

static int sysioctl(int fd, int cmd, void *arg){

}

static int sysexec(int fd){

}

void syscall_handler(struct trap_frame * tfr){

}