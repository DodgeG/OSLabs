# Lab 0: GDB & QEMU 调试 64 位 RISC-V LINUX


## 1 实验目的

- 使用交叉编译工具, 完成Linux内核代码编译
- 使用QEMU运行内核
- 熟悉GDB和QEMU联合调试

## 2 实验环境

- [Ubuntu 22.04.1 LTS]
- [Ubuntu 22.04.1 LTS Windows Subsystem for Linux 2]

## 3 实验步骤

##### 3.1 搭建实验环境

下载并安装VMware，输入许可证密钥激活软件

![image-20220930151335702](C:\Users\13954\AppData\Roaming\Typora\typora-user-images\image-20220930151335702.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930151533846.png)

下载Ubuntu镜像文件，创建虚拟机

<img src="https://images-tc.oss-cn-beijing.aliyuncs.com/20220930151629719.png" alt="image" style="zoom:80%;" />

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930151804338.png)

##### 3.2 获取 Linux 源码和已经编译好的文件系统

在Windows环境下下载Linux源码，使用git工具clone已提供的仓库

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930152617788.png)

在VMw中设置共享文件夹。方便将以上两个文件移到Linux环境下

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930152757021.png)

使用cp命令复制文件夹到Linux下

`cp -r /mnt/hgfs/E/os22 /home/gdz`

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930153642499.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930153725441.png)

解压linux源码压缩包

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930205902289.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930205853056.png)

##### 3.3 编译Linux内核

使用默认配置

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930210008352.png)

编译

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930210017700.png)

##### 3.4 使用QEMU运行内核

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930210055779.png)

退出 QEMU 的方法为：使用 Ctrl+A，**松开**后再按下 X 键即可退出 QEMU。

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930210120040.png)

##### 3.5 使用GDB对内核进行调试

开启两个终端

# Terminal 1
$ qemu-system-riscv64 -nographic -machine virt -kernel ~/os22/linux-5.19.8/arch/riscv/boot/Image \
    -device virtio-blk-device,drive=hd0 -append "root=/dev/vda ro console=ttyS0" \
    -bios default -drive file=~/os22fall-stu/src/lab0/rootfs.img,format=raw,id=hd0 -S -s

# Terminal 2
$ gdb-multiarch path/to/linux/vmlinux
(gdb) target remote :1234   # 连接 qemu
(gdb) b start_kernel        # 设置断点
(gdb) continue              # 继续执行
(gdb) quit                  # 退出 gdb



终端一：

$ qemu-system-riscv64 -nographic -machine virt -kernel ~/os22/linux-5.19.8/arch/riscv/boot/Image \    -device virtio-blk-device,drive=hd0 -append "root=/dev/vda ro console=ttyS0" \    -bios default -drive file=~/os22fall-stu/src/lab0/rootfs.img,format=raw,id=hd0 -S -s

$ qemu-system-riscv64 -nographic -machine virt -kernel ~/os22/linux-5.19.8/arch/riscv/boot/Image \    -device virtio-blk-device,drive=hd0 -append "root=/dev/vda ro console=ttyS0" \    -bios default -drive file=~/os22/os22fall-stu/src/lab0/rootfs.img,format=raw,id=hd0 -S -s

终端二：

`$ gdb-multiarch ~/os22/linux-5.19.8/vmlinux (gdb) target remote :1234   # 连接 qemu`

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930210417547.png)

设置断点、继续执行、退出gdb

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930210548135.png)

`layout asm`：查看汇编代码

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930210636977.png)

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930210642162.png)

`backtrace`：查看函数的调用的栈帧和层级关系

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930210749436.png)

`finish`: 结束当前函数，返回到函数调用点

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930210830678.png)

`frame`: 切换函数的栈帧

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930210902061.png)

`info`: 查看函数内部局部变量的数值，该命令查看寄存器ra的值

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930210942275.png)

`break` 设置断点在0x80200000

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930211016950.png)

`next`: 单步调试（逐过程，函数直接执行）

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930211056490.png)

`quit` 退出gdb调试

![image](https://images-tc.oss-cn-beijing.aliyuncs.com/20220930211135138.png)

## 4 心得与体会

本次实验过程中的我遇到的困难主要是实验环境的搭建。一开始我尝试在Ubuntu子系统下进行编译，但是编译后缺少很多文件，可以推断在windows环境下无法正常编译。所以我转而尝试使用虚拟机进行试验。在创建好Ubuntu虚拟机后，发现无法访问Windows的文件系统。经过搜索发现，需要设置共享文件夹。此处我又犯了一个错误，即通过访问挂载的文件夹进行实验操作，结果还是未能成功编译，经同学提醒后，我将Linux源码和clone下来的仓库移到了Linux虚拟机下，这次成功进行了编译。

### 4.4 使用QEMU运行内核

$ qemu-system-riscv64 -nographic -machine virt -kernel ~/os22/linux-5.19.8/arch/riscv/boot/Image \ -device virtio-blk-device,drive=hd0 -append "root=/dev/vda ro console=ttyS0" \ -bios default -drive file=~/os22fall-stu/src/lab0/rootfs.img,format=raw,id=hd0

### 4.5 使用 GDB 对内核进行调试

# Terminal 1
$ qemu-system-riscv64 -nographic -machine virt -kernel ~/os22/linux-5.19.8/arch/riscv/boot/Image \
    -device virtio-blk-device,drive=hd0 -append "root=/dev/vda ro console=ttyS0" \
    -bios default -drive file=~/os22fall-stu/src/lab0/rootfs.img,format=raw,id=hd0 -S -s

# Terminal 2
$ gdb-multiarch ~/os22/linux-5.19.8/vmlinux
(gdb) target remote :1234   # 连接 qemu
(gdb) b start_kernel        # 设置断点
(gdb) continue              # 继续执行
(gdb) quit                  # 退出 gdb
