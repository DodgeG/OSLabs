#include "syscall.h"
#include "printk.h"

extern struct task_struct *current;

void sys_write(struct pt_regs *regs)
{
    unsigned int fd = regs->reg[9];
    const char *buf = regs->reg[10];
    int count = regs->reg[11];
    uint64_t ret = 0;
    for (int i = 0; i < count; i++)
    {
        if (fd == 1)
        {
            printk("%c", buf[i]);
            ret++;
        }
    }
    regs->reg[9] = ret;
}

void sys_getpid(struct pt_regs *regs)
{
    regs->reg[9] = current->pid;
}