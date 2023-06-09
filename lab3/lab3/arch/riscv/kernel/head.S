.extern start_kernel

    .section .text.init        # 将 _start 放入.text.init section 
    .globl _start
_start:
    la sp, boot_stack_top # 将boot_stack_top的值加载到sp寄存器
    call mm_init # initialize physical memory
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
    li t0,0x2
    csrw sstatus,t0
    # ------------------
    # - your lab1 code -
    la sp, boot_stack_top # 将boot_stack_top的值加载到sp寄存器
    j start_kernel
    # ------------------

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 # <-- change to your stack size

    .globl boot_stack_top
boot_stack_top: