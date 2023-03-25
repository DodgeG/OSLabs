#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include "stdint.h"
#include "proc.h"

#define SYS_WRITE   64
#define SYS_GETPID  172

void sys_write(struct pt_regs *regs);

void sys_getpid(struct pt_regs *regs);

#endif