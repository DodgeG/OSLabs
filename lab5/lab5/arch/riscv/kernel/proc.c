#include "proc.h"
#include "mm.h"
#include "rand.h"
#include "printk.h"
#include "defs.h"
#include "string.h"
#include "vm.h"
#include "elf.h"

extern void __dummy();
extern uint64_t uapp_start;
extern uint64_t uapp_end;
extern unsigned long *swapper_pg_dir;

struct task_struct *idle;           // idle process
struct task_struct *current;        // 指向当前运行线程的 `task_struct`
struct task_struct *task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

static uint64_t load_elf_program(struct task_struct *task)
{
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)(&uapp_start);// 从地址 uapp_start 开始，便是我们要找的 Ehdr

    uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff;// Phdr 数组，其中的每个元素都是一个 Elf64_Phdr
    int phdr_cnt = ehdr->e_phnum;// ELF 文件包含的 Segment 的数量

    Elf64_Phdr *phdr;
    int load_phdr_cnt = 0;
    for (int i = 0; i < phdr_cnt; i++)
    {
        phdr = (Elf64_Phdr *)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD)
        {
            // alloc space and copy content
            uint64_t pg_num = (PGOFFSET(phdr->p_vaddr) + phdr->p_memsz - 1) / PGSIZE + 1;// page amount of segment
            uint64_t seg_page = alloc_pages(pg_num);
            uint64_t seg_addr = ((uint64_t)(&uapp_start) + phdr->p_offset);
            memcpy((void *)(seg_page + PGOFFSET(phdr->p_vaddr)), (void *)(seg_addr), phdr->p_memsz);
            // do mapping
            create_mapping((uint64 *)PA2VA((uint64_t)task->pgd), PGROUNDDOWN(phdr->p_vaddr), VA2PA(seg_page), pg_num * PGSIZE, phdr->p_flags << 1 | 16);
        }
    }

    // allocate user stack and do mapping
    uint64_t u_stack_begin = alloc_page();
    create_mapping((uint64 *)PA2VA((uint64_t)task->pgd), USER_END - PGSIZE, VA2PA(u_stack_begin), PGSIZE, 23);

    // set user stack
    task->thread_info.user_sp = USER_END;
    // pc for the user program
    task->thread.sepc = ehdr->e_entry; // 程序的第一条指令被存储的用户态虚拟地址
    // sstatus bits set
    // SPP = 0, SPIE = 1, SUM = 1
    task->thread.sstatus = (1 << 18) | (1 << 5);
    // user stack for user program
    task->thread.sscratch = USER_END;
}

void task_init()
{
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    idle = (struct task_struct *)kalloc();
    // 2. 设置 state 为 TASK_RUNNING;
    idle->state = TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    idle->counter = 0;
    idle->priority = 0;
    // 4. 设置 idle 的 pid 为 0
    idle->pid = 0;
    // 5. 将 current 和 task[0] 指向 idle
    current = idle;
    task[0] = idle;

    // set idle's pgd to kernel pagetable
    idle->pgd = swapper_pg_dir;
    idle->thread.sscratch = 0;

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    for (int i = 1; i < NR_TASKS; i++)
    {
        task[i] = (struct task_struct *)kalloc();
        // 2. 其中每个线程的 state 为 TASK_RUNNING, counter 为 0, priority 使用 rand() 来设置, pid 为该线程在线程数组中的下标。
        task[i]->state = TASK_RUNNING;
        task[i]->counter = 0;
        task[i]->priority = rand() % PRIORITY_MAX + 1;
        task[i]->pid = i;
        // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
        // 4. 其中 `ra` 设置为 __dummy 的地址,  `sp` 设置为 该线程申请的物理页的高地址
        task[i]->thread.ra = (uint64)__dummy;
        task[i]->thread.sp = (uint64)task[i] + PGSIZE - 1;

        // allocate task pagetable
        task[i]->pgd = (pagetable_t)alloc_page();
        // copy kernel pagetable to task pagetable
        memcpy((void *)(task[i]->pgd), (void *)((&swapper_pg_dir)), PGSIZE);
        // change to physical address
        task[i]->pgd = (pagetable_t)VA2PA((uint64_t)task[i]->pgd); 

        load_elf_program(task[i]);
    }

    printk("...proc_init done!\n");
}

extern void __switch_to(struct task_struct *prev, struct task_struct *next);

void switch_to(struct task_struct *next)
{
    if (next == current)
        return; // switching to current is needless
    else
    {
        struct task_struct *current_saved = current;
        current = next;
        __switch_to(current_saved, next);
    }
}

void do_timer(void)
{
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度
    if (current == task[0])
        schedule();
    else
    {
        current->counter -= 1;
        if (current->counter == 0)
            schedule();
    }
}

// #define DSJF
#ifdef DSJF
void schedule(void)
{
    uint64_t min_count = INF;
    struct task_struct *next = NULL;
    char all_zeros = 1;
    for (int i = 1; i < NR_TASKS; i++)
    {
        if (task[i]->state == TASK_RUNNING && task[i]->counter > 0)
        {
            if (task[i]->counter < min_count)
            {
                min_count = task[i]->counter;
                next = task[i];
            }
            all_zeros = 0;
        }
    }

    if (all_zeros)
    {
        printk("\n");
        for (int i = 1; i < NR_TASKS; i++)
        {
            task[i]->counter = rand() % 10 + 1;
            printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
        }
        schedule();
    }
    else
    {
        if (next)
        {
            printk("\nswitch to [PID = %d COUNTER = %d]\n", next->pid, next->counter);
            switch_to(next);
        }
    }
}
#endif

#ifdef DPRIORITY
void schedule(void)
{
    uint64_t c, i, next;
    struct task_struct **p;
    while (1)
    {
        c = 0;
        next = 0;
        i = NR_TASKS;
        p = &task[NR_TASKS];
        while (--i)
        {
            if (!*--p)
                continue;
            if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
                c = (*p)->counter, next = i;
        }

        if (c)
            break;
        // else all counters are 0s
        printk("\n");
        for (p = &task[NR_TASKS - 1]; p > &task[0]; --p)
        {
            if (*p)
            {
                (*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
                printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", (*p)->pid, (*p)->priority, (*p)->counter);
            }
        }
    }

    printk("\nswitch to [PID = %d PRIORITY = %d COUNTER = %d]\n", task[next]->pid, task[next]->priority, task[next]->counter);
    switch_to(task[next]);
}
#endif