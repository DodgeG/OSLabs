#include "defs.h"
    .extern start_kernel
    .section .text.init        # 将 _start 放入.text.init section 
    .globl _start
_start:
    la sp, boot_stack_top # 将boot_stack_top的值加载到sp寄存器
    li t0, PA2VA_OFFSET
    sub sp, sp, t0

    call setup_vm # set up vitual memory
    call relocate

    call mm_init # initialize physical memory
    call setup_vm_final # set up real virtual memory
    call task_init # initialize task threads

    # set stvec = _traps
    la t0, _traps
    csrw stvec, t0

    # set sie[STIE] = 1
    li t0, 0x20
    csrw sie, t0 

    # set first time interrupt
    rdtime t0
    li t1, 10000000
    add t2, t0, t1
    mv a0, t2
    li a1, 0
    li a2, 0
    li a3, 0
    li a4, 0
    li a5, 0
    li a6, 0 # fid
    li a7, 0 # ext
    ecall

    # set sstatus[SIE] = 1
    li t0, 0x2
    csrw sstatus, t0

    j start_kernel

relocate:
    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)
    li t0, PA2VA_OFFSET

    add ra, ra, t0 # set ra = ra + PA2VA_OFFSET
    add sp, sp, t0 # set sp = sp + PA2VA_OFFSET

    # set satp with early_pgtbl
    la t1, early_pgtbl
    sub t1, t1, t0 # PA of early_pgtbl
    srli t1, t1, 12 # PA >> 12 = PPN
    li t2, 8 # MODE = 8
    slli t2, t2, 60 # MODE: satp[63:60]
    or t1, t1, t2
    csrw satp, t1
    
    # flush tlb
    sfence.vma zero, zero

    # flush icache
    fence.i

    ret

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 0x1000 # <-- change to your stack size

    .globl boot_stack_top
boot_stack_top: