# Lab 3: RV64 内核线程调度


## 1 实验目的

- 了解线程概念, 并学习线程相关结构体, 并实现线程的初始化功能。
- 了解如何使用时钟中断来实现线程的调度。
- 了解线程切换原理, 并实现线程的切换。
- 掌握简单的线程调度算法, 并完成两种简单调度算法的实现。

## 2 实验环境

- Environment in previous labs

## 3 背景知识

### 3.0 前言

在 lab2 中, 我们利用 trap 赋予了 OS 与软件, 硬件的交互能力。但是目前我们的 OS 还不具备多进程调度以及并发执行的能力。在本次实验中, 我们将利用时钟中断, 来实现多进程的调度以使得多个进程/线程并发执行。

### 3.1 进程与线程

`源代码`经编译器一系列处理（编译、链接、优化等）后得到的可执行文件, 我们称之为`程序 (Program)`。而通俗地说, `进程`就是`正在运行并使用计算机资源`的程序。`进程`与`程序`的不同之处在于, `进程`是一个动态的概念, 其不仅需要将其运行的程序的代码/数据等加载到内存空间中, 还需要拥有自己的`运行栈`。同时一个`进程`可以对应一个或多个`线程`, `线程`之间往往具有相同的代码, 共享一块内存, 但是却有不同的CPU执行状态。

在本次实验中, 为了简单起见, 我们采用 `single-threaded process` 模型, 即`一个进程`对应`一个线程`, 进程与线程不做明显区分。

### 3.1 线程相关属性

在不同的操作系统中, 为每个线程所保存的信息都不同。在这里, 我们提供一种基础的实现, 每个线程会包括：

- `线程ID`：用于唯一确认一个线程。
- `运行栈`：每个线程都必须有一个独立的运行栈, 保存运行时的数据。
- `执行上下文`：当线程不在执行状态时, 我们需要保存其上下文（其实就是`状态寄存器`的值）, 这样之后才能够将其恢复, 继续运行。
- `运行时间片`：为每个线程分配的运行时间。
- `优先级`：在优先级相关调度时, 配合调度算法, 来选出下一个执行的线程。

### 3.2 线程切换流程图

- 在每次处理时钟中断时, 操作系统首先会将当前线程的运行剩余时间减少一个单位。之后根据调度算法来确定是继续运行还是调度其他线程来执行。
- 在进程调度时, 操作系统会遍历所有可运行的线程, 按照一定的调度算法选出下一个执行的线程。最终将选择得到的线程与当前线程切换。
- 在切换的过程中, 首先我们需要保存当前线程的执行上下文, 再将将要执行线程的上下文载入到相关寄存器中, 至此我们就完成了线程的调度与切换。

## 4 实验步骤

### 4.1 准备工程

- 此次实验基于 lab2 同学所实现的代码进行。
- 在 lab3 中需要同学需要添加并修改 `arch/riscv/include/proc.h` `arch/riscv/kernel/proc.c` 两个文件。

### 4.2 `proc.h` 数据结构定义

```c
// arch/riscv/include/proc.h

#include "types.h"

#define NR_TASKS  (1 + 31) // 用于控制 最大线程数量 （idle 线程 + 31 内核线程）

#define TASK_RUNNING    0 // 为了简化实验, 所有的线程都只有一种状态

#define PRIORITY_MIN 1
#define PRIORITY_MAX 10

/* 用于记录 `线程` 的 `内核栈与用户栈指针` */
/* (lab3中无需考虑, 在这里引入是为了之后实验的使用) */
struct thread_info {
    uint64 kernel_sp;
    uint64 user_sp;
};

/* 线程状态段数据结构 */
struct thread_struct {
    uint64 ra;
    uint64 sp;
    uint64 s[12];
};

/* 线程数据结构 */
struct task_struct {
    struct thread_info* thread_info;
    uint64 state;    // 线程状态
    uint64 counter;  // 运行剩余时间
    uint64 priority; // 运行优先级 1最低 10最高
    uint64 pid;      // 线程id

    struct thread_struct thread;
};

/* 线程初始化 创建 NR_TASKS 个线程 */
void task_init();

/* 在时钟中断处理中被调用 用于判断是否需要进行调度 */
void do_timer();

/* 调度程序 选择出下一个运行的线程 */
void schedule();

/* 线程切换入口函数*/
void switch_to(struct task_struct* next);

/* dummy funciton: 一个循环程序, 循环输出自己的 pid 以及一个自增的局部变量 */
void dummy();

```



### 4.3 线程调度功能实现

#### 4.3.1 线程初始化

在初始化线程时以 4KB 为粒度，按照每个 Task 一帧的形式进行分配，并将`task_struct`存放在该页的低地址部分， 将线程的栈指针`sp`指向该页的高地址。

首先要为`idle`设置`task_struct`，并将`current`和`task[0]`都指向`idle`。

然后要将`task[1]`~`task[NR_TASKS - 1]`全部初始化，和`idle`设置的区别在于要为这些线程设置 `thread_struct`中的`ra`和`sp`。ra被初始化为dummy，sp指向页的最高处地址

```c
void task_init() {
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    idle=(struct task_struct*)kalloc();
    // 2. 设置 state 为 TASK_RUNNING;
    idle->state=TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    idle->counter=0;
    idle->priority=0;
    // 4. 设置 idle 的 pid 为 0
    idle->pid=0;
    // 5. 将 current 和 task[0] 指向 idle
    current=idle;
    task[0]=idle;

    
    for (int i=1;i<=NR_TASKS-1;i++){
        task[i]=(struct task_struct*)kalloc();
        task[i]->state=TASK_RUNNING;
        task[i]->counter=0;
        task[i]->priority=rand()%PRIORITY_MAX+1;
        task[i]->pid=i;
        task[i]->thread.ra=(uint64)__dummy;
        task[i]->thread.sp=(uint64)task[i]+PGSIZE-1;
    }

    printk("...proc_init done!\n");
}
```



#### 4.3.2 `__dummy` 与 `dummy` 介绍

- `task[1]` ~ `task[NR_TASKS - 1]`都运行同一段代码 `dummy()` 我们在 `proc.c` 添加 `dummy()`:

  ```c
  
  void dummy() {
      uint64 MOD = 1000000007;
      uint64 auto_inc_local_var = 0;
      int last_counter = -1;
      while(1) {
          if (last_counter == -1 || current->counter != last_counter) {
              last_counter = current->counter;
              auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
              printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
          }
      }
  }
  ```

  

- 在 `entry.S` 添加 `__dummy`

  - 在`__dummy` 中将 sepc 设置为 `dummy()` 的地址, 并使用 `sret` 从中断中返回。

    ```asm
    __dummy:
        # YOUR CODE HERE
        la t0, dummy
        csrw sepc, t0
        sret
    ```

     

#### 4.3.3 实现线程切换

判断下一个执行的线程 `next` 与当前的线程 `current` 是否为同一个线程, 如果是同一个线程, 则无需做任何处理, 否则调用 `__switch_to` 进行线程切换。



```c
extern void __switch_to(struct task_struct* prev, struct task_struct* next);

void switch_to(struct task_struct* next) {
    /* YOUR CODE HERE */
    if (next==current)
        return;
    else {
#ifdef DSJF
        printk("\n");
        printk("switch to [PID = %d COUNTER = %d]\n", next->pid, next->counter);
#endif

#ifdef DPRIORITY
        printk("\n");
        printk("switch to [PID = %d PRIORITY = %d COUNTER = %d]\n", next->pid, next->priority, next->counter);
#endif       
        struct task_struct* prev = current;
        current=next;
        __switch_to(prev,next);
    }
}
```

```asm
__switch_to:
    # save state to prev process
    # YOUR CODE HERE
    add t0, a0, 40  # offset of thread_struct in task_struct
    sd ra, 0(t0)
    sd sp, 8(t0)
    sd s0, 16(t0)
    sd s1, 24(t0)
    sd s2, 32(t0)
    sd s3, 40(t0)
    sd s4, 48(t0)
    sd s5, 56(t0)
    sd s6, 64(t0)
    sd s7, 72(t0)
    sd s8, 80(t0)
    sd s9, 88(t0)
    sd s10, 96(t0)
    sd s11, 104(t0)
    # restore state from next process
    # YOUR CODE HERE
    add t0, a1, 40  # offset of thread_struct next task_struct
    ld ra, 0(t0)
    ld sp, 8(t0)
    ld s0, 16(t0)
    ld s1, 24(t0)
    ld s2, 32(t0)
    ld s3, 40(t0)
    ld s4, 48(t0)
    ld s5, 56(t0)
    ld s6, 64(t0)
    ld s7, 72(t0)
    ld s8, 80(t0)
    ld s9, 88(t0)
    ld s10, 96(t0)
    ld s11, 104(t0)

    ret

```

注意在存取和恢复thread_struct之前，需要有8*5个字节的偏移，原因是task_struct中还有五个八字节的变量

```c
struct task_struct {
    struct thread_info* thread_info;
    uint64 state;    // 线程状态
    uint64 counter;  // 运行剩余时间
    uint64 priority; // 运行优先级 1最低 10最高
    uint64 pid;      // 线程id

    struct thread_struct thread;
};
```



#### 4.3.4 实现调度入口函数

实现 `do_timer()`, 并在 `时钟中断处理函数` 中调用。

```c
void do_timer(void) {
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度
    if (current==idle)
        schedule();
    else{
        current->counter--;
        if (current->counter>0)
            return;
        else
            schedule();
    }
    /* YOUR CODE HERE */
}
```



#### 4.3.5 实现线程调度

本次实验我们需要实现两种调度算法：1.短作业优先调度算法, 2.优先级调度算法。

##### 4.3.5.1 短作业优先调度算法

- 当需要进行调度时按照一下规则进行调度：

  - 遍历线程指针数组`task`(不包括 `idle` , 即 `task[0]` ), 在所有运行状态 (`TASK_RUNNING`) 下的线程运行剩余时间`最少`的线程作为下一个执行的线程。
  - 如果`所有`运行状态下的线程运行剩余时间都为0, 则对 `task[1]` ~ `task[NR_TASKS-1]` 的运行剩余时间重新赋值 (使用 `rand()`) , 之后再重新进行调度。

  ```c
  void schedule(void)
  {
      struct task_struct *next = current;
      uint64 min_counter = 0xFFFFFFFFFFFFFFFF;
      while (1)
      {
          //遍历线程指针数组task(不包括 idle , 即 task[0] ), 在所有运行状态 (TASK_RUNNING) 下的线程运行剩余时间最少的线程作为下一个执行的线程。
          for (int i = 1; i < NR_TASKS; i++)
          {
              if (task[i]->state == TASK_RUNNING && task[i]->counter > 0)
              {
                  if (task[i]->counter < min_counter)
                  {
                      min_counter = task[i]->counter;
                      next = task[i];
                  }
              }
          }
          //如果所有运行状态下的线程运行剩余时间都为0, 则对 task[1] ~ task[NR_TASKS-1] 的运行剩余时间重新赋值 (使用 rand()) , 之后再重新进行调度。
          if (min_counter == 0xFFFFFFFFFFFFFFFF)
          {
              for (int i = 1; i < NR_TASKS; i++)
              {
                  printk("\n");
                  task[i]->counter = rand() % 10 + 2;
                  printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
              }
          }
          else
              break;
      }
      switch_to(next);
  }
  ```

  

##### 4.3.5.2 优先级调度算法

与短作业优先调度算法的唯一区别在于，将运行状态下的线程优先级最高的线程作为下一个执行的线程

```c
void schedule(void)
{
    struct task_struct *next = current;
    uint64 max_priority = PRIORITY_MIN - 1;
    while (1)
    {
        //将优先级最高的线程作为下一个执行的线程
        for (int i = 1; i < NR_TASKS; i++)
        {
            if (task[i]->state == TASK_RUNNING && task[i]->counter > 0)
            {
                if (task[i]->priority > max_priority)
                {
                    max_priority = task[i]->priority;
                    next = task[i];
                }
            }
        }
        //运行剩余时间都为0,重新赋值再进行调度
        if (max_priority == PRIORITY_MIN - 1)
        {
            printk("\n");
            for (int i = 1; i < NR_TASKS; i++)
            {
                task[i]->counter = rand() % 10 + 2;
                printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", task[i]->pid, task[i]->priority, task[i]->counter);
            }
        }
        else
            break;
    }
    switch_to(next);
}
```



### 4.4 编译及测试

短作业优先调度

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221116151859845.png)

优先级调度

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221116151407234.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221116151427775.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221116151537968.png)

## 思考题

1. 在 RV64 中一共用 32 个通用寄存器, 为什么 `context_switch` 中只保存了14个?

   s0-s11,ra,sp是callee saved register，需要自行保存。其他寄存器属于caller saved register，在c语言调用中会被保存在栈中。
   

2. 当线程第一次调用时, 其 `ra` 所代表的返回点是 `__dummy`。那么在之后的线程调用中 `context_switch` 中, `ra` 保存/恢复的函数返回点是什么呢? 请同学用 gdb 尝试追踪一次完整的线程切换流程, 并关注每一次 `ra` 的变换 (需要截图)。

   首先在__switch_to处打断点，

   ![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221116133147598.png)

   运行到断点后查看汇编代码

​		![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221116133350809.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221116133411577.png)

在ra的恢复和保存处打断点，即0x802001a0和0x802001dc

清除__switch_to处的断点

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221116133717079.png)

第一次保存的返回点是switch_to+132,第一次恢复的返回点是__dummy

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221116141023291.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221116141138188.png)

之后每次保存和恢复的返回点都是switch_to+132

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221116141436957.png)

可以看到switch_to+132为__switch_to的下一行

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221116144249760.png)
