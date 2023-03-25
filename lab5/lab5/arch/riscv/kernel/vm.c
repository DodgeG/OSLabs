// arch/riscv/kernel/vm.c
#include "defs.h"
#include "types.h"
#include "vm.h"
#include "mm.h"
#include "string.h"
#include "printk.h"

/* early_pgtbl: 用于 setup_vm 进行 1GB 的 映射。 */
unsigned long early_pgtbl[512] __attribute__((__aligned__(0x1000)));

void setup_vm(void)
{
    /*
    1. 由于是进行 1GB 的映射 这里不需要使用多级页表
    2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
        high bit 可以忽略
        中间9 bit 作为 early_pgtbl 的 index
        低 30 bit 作为 页内偏移 这里注意到 30 = 9 + 9 + 12， 即我们只使用根页表， 根页表的每个 entry 都对应 1GB 的区域。
    3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    */
    // virtual address = 0x80000000 => VPN[2] = 2

    memset(early_pgtbl, 0x0, PGSIZE);
    uint64 index, entry;
    // PA == VA
    // index = PHY_START[38:30]
    index = ((uint64)PHY_START & 0x0000007FC0000000) >> 30;
    // entry[53:28] = PHY_START[55:30], entry[3:0] = 0xF
    entry = ((PHY_START & 0x00FFFFFFC0000000) >> 2) | 0xF;
    early_pgtbl[index] = entry;

    // PA + PV2VA_OFFSET == VA
    // index = VM_START[38:30]
    index = ((uint64)VM_START & 0x0000007FC0000000) >> 30;
    // entry[53:28] = PHY_START[55:30], entry[3:0] = 0xF
    entry = ((PHY_START & 0x00FFFFFFC0000000) >> 2) | 0xF;
    early_pgtbl[index] = entry;
    printk("...setup_vm done!\n");
}

/* swapper_pg_dir: kernel pagetable 根目录， 在 setup_vm_final 进行映射。 */
unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

extern uint64 _stext;
extern uint64 _srodata;
extern uint64 _sdata;

void setup_vm_final(void)
{
    memset(swapper_pg_dir, 0x0, PGSIZE);

    // No OpenSBI mapping required
    // mapping kernel text X|-|R|V
    create_mapping((uint64 *)swapper_pg_dir, (uint64)&_stext, (uint64)(&_stext) - PA2VA_OFFSET, ((uint64)(&_srodata) - (uint64)(&_stext)), 11);

    // mapping kernel rodata -|-|R|V
    create_mapping((uint64 *)swapper_pg_dir, (uint64)&_srodata, (uint64)(&_srodata) - PA2VA_OFFSET, ((uint64)(&_sdata) - (uint64)(&_srodata)), 3);

    // mapping other memory -|W|R|V
    create_mapping((uint64 *)swapper_pg_dir, (uint64)&_sdata, (uint64)(&_sdata) - PA2VA_OFFSET, (PHY_END + PA2VA_OFFSET - (uint64)(&_sdata)) , 7);

    uint64 satp = (((uint64)swapper_pg_dir - PA2VA_OFFSET) >> 12) | (uint64)0x8 << 60;
    // set satp with swapper_pg_dir
    __asm__ volatile("csrw satp, %[base]" ::[base] "r"(satp)
                     :);

    // flush TLB
    asm volatile("sfence.vma zero, zero");

    // flush icache
    asm volatile("fence.i");

    printk("...setup_vm_final done!\n");
}

/* 创建多级页表映射关系 */
/*
    pgtbl 为根页表的基地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的大小
    perm 为映射的读写权限
*/
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
    /*
    创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
    可以使用 V bit 来判断页表项是否存在
    */
    sz /= PGSIZE;
    while (sz--)
    {
        uint64 vpn2 = ((va & 0x7fc0000000) >> 30);
        uint64 vpn1 = ((va & 0x3fe00000) >> 21);
        uint64 vpn0 = ((va & 0x1ff000) >> 12);

        // the second level page (next to root)
        uint64 *pgtbl1;
        if (!(pgtbl[vpn2] & 1))
        {
            pgtbl1 = (uint64 *)kalloc();
            pgtbl[vpn2] |= (1 | (((uint64)pgtbl1 - PA2VA_OFFSET) >> 2));
        }
        else
            pgtbl1 = (uint64 *)(PA2VA_OFFSET + ((pgtbl[vpn2] & 0x3ffffffffffffc00) << 2));

        // the third level page
        uint64 *pgtbl0;
        if (!(pgtbl1[vpn1] & 1))
        {
            pgtbl0 = (uint64 *)kalloc();
            pgtbl1[vpn1] |= (1 | (((uint64)pgtbl0 - PA2VA_OFFSET) >> 2));
        }
        else
            pgtbl0 = (uint64 *)(PA2VA_OFFSET + ((pgtbl1[vpn1] & 0x3ffffffffffffc00) << 2));

        // the physical page
        // note the perm only contains infomation about UXWRV
        pgtbl0[vpn0] = (1 | (perm) | (pa >> 2));

        va += 0x1000, pa += 0x1000;
    }
}
