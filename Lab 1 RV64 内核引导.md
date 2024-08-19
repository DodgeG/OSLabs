# Lab 1: RV64 内核引导


## 1 实验目的

- 学习 RISC-V 汇编， 编写 head.S 实现跳转到内核运行的第一个 C 函数。
- 学习 OpenSBI，理解 OpenSBI 在实验中所起到的作用，并调用 OpenSBI 提供的接口完成字符的输出。
- 学习 Makefile 相关知识， 补充项目中的 Makefile 文件， 来完成对整个工程的管理。

## 2 实验环境

- Environment in Lab0

## 3 实验步骤

### 3.1 准备工程

学习riscv汇编、makefile、内联汇编等知识，完善以下文件：

- arch/riscv/kernel/head.S
- lib/Makefile
- arch/riscv/kernel/sbi.c
- lib/print.c

- arch/riscv/include/defs.h

需要实现调用sbi_ecall，完成字符串输出puts()和puti()的实现。

### 3.2 编写head.S

```assembly
.extern start_kernel

    .section .text.entry
    .globl _start
_start:
    # ------------------
    # - your code here -
    la sp, boot_stack_top
    j start_kernel
    # ------------------

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 0x1000 # <-- change to your stack size

    .globl boot_stack_top
boot_stack_top:
```

首先`la sp, boot_stack_top`将boot_stack_top载入保存栈指针的寄存器sp，实现了将该栈放置在`.bss.stack` 段。`j start_kernel`通过跳转指令跳转到`start_kernel` 函数

### 3.3 完善 Makefile 脚本

```makefile
all : print.o

print.o : print.c
	$(GCC) $(CFLAG) -o print.o -c print.c

clean :
	rm print.o
```

链接print.c生成print.o文件

### 3.4 补充 `sbi.c`

```c
#include "types.h"
#include "sbi.h"


struct sbiret sbi_ecall(int ext, int fid, uint64 arg0,
			            uint64 arg1, uint64 arg2,
			            uint64 arg3, uint64 arg4,
			            uint64 arg5) 
{
    // unimplemented  
	struct sbiret res;

	__asm__ volatile(
		"mv a0, %[arg0]\n"
		"mv a1, %[arg1]\n"
		"mv a2, %[arg2]\n"
		"mv a3, %[arg3]\n"
		"mv a4, %[arg4]\n"
        "mv a5, %[arg5]\n"
		"mv a6, %[fid]\n"
		"mv a7, %[ext]\n"
		"ecall\n"
		"mv %[error], a0\n"
		"mv %[value], a1\n"
		:[error] "=r" (res.error), [value] "=r" (res.value)
		:[arg0] "r" (arg0), [arg1] "r" (arg1), [arg2] "r" (arg2), [arg3] "r" (arg3), [arg4] "r" (arg4), [arg5] "r" (arg5), [ext] "r" (ext), [fid] "r" (fid)
		: "memory", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7" 
	);
	return res;      
}

```

​		

```C
		"mv a0, %[arg0]\n"
		"mv a1, %[arg1]\n"
		"mv a2, %[arg2]\n"
		"mv a3, %[arg3]\n"
		"mv a4, %[arg4]\n"
        "mv a5, %[arg5]\n"
		"mv a6, %[fid]\n"
		"mv a7, %[ext]\n"
```

将参数arg0-arg7分别存入a0-a5寄存器，将fid ext存入a6 a7

```c
		"ecall\n"
```

调用ecall函数，将七个参数传给ecall，进行字符串输出操作

```c
	"mv %[error], a0\n"
	"mv %[value], a1\n"
```

ecall执行后的返回值保存在a0 a1，将返回值存入error和value

```c
		:[error] "=r" (res.error), [value] "=r" (res.value)
```

输出结果存入res.error、res.value

```c
		:[arg0] "r" (arg0), [arg1] "r" (arg1), [arg2] "r" (arg2), [arg3] "r" (arg3), [arg4] "r" (arg4), [arg5] "r" (arg5), [ext] "r" (ext), [fid] "r" (fid)
```

输入arg0-arg5、ext、fid等分别放入临时变量[arg0],[ext],[fid]

```c
: "memory", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7" 
```

这句表示进行过输出输出的寄存器和内存有以上几个

### 3.5 `puts()` 和 `puti()`

```c
#include "print.h"
#include "sbi.h"

void puts(char *s) {
    // unimplemented
    while(*s)
    {
        sbi_ecall(0x1, 0x0, *s, 0, 0, 0, 0, 0);
        s++;
    }
}

void puti(int x) {
    // unimplemented
    int bit[100];
    int count = 0;
    while(x)
    {
        bit[count++] = x % 10;
        x /= 10;
    }
    for(int i = count - 1; i >= 0; i--)
        sbi_ecall(0x1, 0x0, bit[i]+'0', 0, 0, 0, 0, 0);
}

```

调用sbi_ecall打印字符，前三个参数分别代表ExtensionID，FunctionID，ascii码

### 3.6 修改 defs

```c
#ifndef _DEFS_H
#define _DEFS_H

#include "types.h"

#define csr_read(csr)                       \
({                                          \
    register uint64 __v;                    \
    /* unimplemented */                     \
    asm volatile ("csrr %0, " #csr          \
                    : "=r"(__v)             \
                    :                       \
                    : "memory"   );         \
    __v;                                    \
})

#define csr_write(csr, val)                         \
({                                                  \
    uint64 __v = (uint64)(val);                     \
    asm volatile ("csrw " #csr ", %0"               \
                    : : "r" (__v)                   \
                    : "memory");                    \
})

#endif
```

## 4 思考题

##### 1 请总结一下 RISC-V 的 calling convention，并解释 Caller / Callee Saved Register 有什么区别？

RISC-V 的 calling convention：

1. 如果函数的参数是结构体成员，那么每个参数要按照所在平台上指针类型大小对齐。结构体至多8个成员会被放在寄存器中，剩余的部分被存放在栈上，sp指向第一个没有被存放在寄存器上的结构体成员。
2. 小于1个指针字长的参数被存在寄存器的低位，如果存在栈上也是存放在内存的低地址上。
3. 函数返回值如果是基本类型或者只包含一两个成员的基本结构体的成员，存放在相应的整形寄存器a0和a1，浮点寄存器fa0和fa1. 大于两个指针字长的返回值存放在内存上。
4. 栈是从高地址向低地址方向的， 并且是16字节对齐的。
5. 寄存器t0~t6~，~ft0~ft11被称为临时寄存器，由调用者保存。s0~s11, fs0~fs11由被调者保存。



Callee-saved register用于保存应在每次调用中保留的长寿命值。当调用者进行过程调用时，可以期望这些寄存器在被调用者返回后将保持相同的值，这使被调用者有责任在返回调用者之前保存它们并恢复它们。


Caller-saved register用于保存不需要在各个调用之间保留的临时数据。因此，如果要在过程调用后恢复该值，则调用方有责任将这些寄存器压入堆栈或将其复制到其他位置。不过，让调用销毁这些寄存器中的临时值是正常的。从被调用方的角度来看，函数可以自由覆盖（也就是破坏）这些寄存器，而无需保存/恢复。


##### 2 编译之后，通过 System.map 查看 vmlinux.lds 中自定义符号的值（截图）

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221013215216968.png)

##### 3 用 `csr_read` 宏读取 `sstatus` 寄存器的值，对照 RISC-V 手册解释其含义（截图）。

调用宏读取sstatus的值

```c
#include "print.h"
#include "sbi.h"
#include "defs.h"

extern void test();

int start_kernel() {
    uint64 res = csr_read(sstatus);
    puti(res);
    csr_write(sscratch, 0x0100);
    
    test(); // DO NOT DELETE !!!

	return 0;
}
```

查看寄存器的值

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014000945136.png)

查看运行结果（读取的值)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221013233939086.png)

sstatus是一个SXLEN-bit 读写寄存器，状态寄存器用来存放两类信息：一类是体现当前指令执行结果的各种状态信息，另一类是存放控制信息

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221013234058650.png)

##### 4 用 `csr_write` 宏向 `sscratch` 寄存器写入数据，并验证是否写入成功（截图）。

```c
csr_write(sscratch, 0x0100); 
```

向sscratch写入256

查看sscratch的值

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014001017247.png)

##### 5 Detail your steps about how to get `arch/arm64/kernel/sys.i`

安装交叉编译工具链

```
sudo apt install gcc-aarch64-linux-gnu
```

获得编译预处理产物

```
# 先 config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig

# 然后指定要生成的文件
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- ./arch/arm64/kernel/sys.i
```

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014001807708.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014003706198.png)

##### 6 Find system call table of Linux v6.0 for `ARM32`, `RISC-V(32 bit)`, `RISC-V(64 bit)`, `x86(32 bit)`, `x86_64` List source code file, the whole system call table with macro expanded, screenshot every step.

```shell
sudo find . -name '*.tbl'
sudo find . -name "syscall*"
```

ARM32:

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014003906490.png)

risc-v(32/64):(不含tbl文件）

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014004154898.png)

x86(32/64):

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014004244047.png)

##### 7 Explain what is ELF file? Try readelf and objdump command on an ELF file, give screenshot of the output. Run an ELF file and cat `/proc/PID /maps` to give its memory layout.

ELF 是一类文件类型，而不是特指某一后缀的文件。 ELF 文件格式在 Linux 下主要有如下三种文件：可执行文件（.out）、可重定位文件、（.o文件）共享目标文件（.so）

ELF文件由4部分组成，分别是ELF头、程序头表、节和节头表。

编写一个cpp程序

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014015347137.png)

使用g++进行编译

`g++ -o elf elf.cpp`

使用readelf读取hello可执行文件

readelf -a elf

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014015510495.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014015533317.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014015554984.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014015609244.png)



使用objdump命令：

objdump -x elf

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014015656155.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014015710653.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014015722738.png)

编译运行elf.c

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014015824751.png)

开启另一个终端，查看PID

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014015851922.png)

PID为14294

查看memory layout.

`cat /proc/14294/maps`

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20221014015932911.png)
