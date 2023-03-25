#include "printk.h"
#include "types.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

void trap_handler(uint64 scause, uint64 sepc, struct pt_regs *regs)
{
    // Supervisor timer interrupt: Interrupt = 1, Exc Code = 5
    if (scause == 0x8000000000000005)
    {
        clock_set_next_event();
        do_timer();
    }
    // ECALL_FROM_U_MODE exception: Interrupt = 0, Exc Code = 8
    else if (scause == 0x0000000000000008)
    {
        // reg[16]: a7 (Extension ID), reg[9]: a0 (return value)
        switch (regs->reg[16])
        {
        case SYS_WRITE:
            sys_write(regs);
            break;
        case SYS_GETPID:
            sys_getpid(regs);
            break;
        }
        // 异常处理完成之后，继续执行后续的指令
        regs->sepc += 4;
    }
}
