#伙伴系统的实现

Vincent Zhao *vincentzhaorz@pku.edu.cn*

伙伴系统主要由一个保存内存块`struct MemoryBlock`的数组组成。`MemoryBlock`是一个描述内存中连续的具有一定大小的块的数据结构，包含起始的虚拟地址`va`和块大小`size`。这个数组`bd_sys_memblocks`保存在`UBDSYS`这个位置，该位置在原来的`UENVS`处，`UENVS`向下平移一个`PTSIZE`大小的空间。

伙伴系统的功能就是基于这个数组实现的。在这个数组之上还维护这一个大小为`MAX_BD_ORDER`的`struct MemoryBlock*`的数组`bd_sys_binary`，这个数组中每个偏移量为i的元素都是指向大小为$$2^i$$的`MemoryBlock`。这个数据结构用来对内存块按照大小来组织，便于之后的合并和拆分。

###伙伴系统的启动
伙伴系统的第一个函数是`bd_sys_start`，该函数接受两个参数：`va`指代伙伴系统的起始地址，这个地址必须在`UTEXT`和`UTOP`之间，而且经过测试发现，这个起始位置至少要在`UTEXT+PTSIZE`的位置，否则伙伴系统会把程序代码覆盖掉。`size`指代的是伙伴系统的大小，这个大小一定要是2的某次幂，并且不能超过`MAX_BD_SIZE`。这个函数会初始化`bd_sys_memblocks`，用一个`bd_sys_free_list`把其中的块串起来。在整个函数的最后，从`bd_sys_free_list`中取一个空闲的`MemoryBlock`出来，把它的起始虚拟地址设为`va`，大小设为`size`，作为伙伴系统的初始内存块。

###对MemoryBlock的操作
然后就是对`bd_sys_memblocks`这个数组的辅助函数了：

* `bd_sys_alloc`从伙伴系统的`bd_sys_free_list`中取一个内存块，赋给其起始地址和大小，然后把指针返回。
* `bd_sys_free`函数的作用是把一个`muemblock`重新放回到`bd_sys_free_list`中。

###划分(split)与合并(coalescence)

之后是关键的划分与合并功能：

* **划分**：`bd_sys_split`函数是递归调用的，接受参数为order，即想要**尝试划分**几阶大小的内存块。如果尝试成功了，则进行划分，把当前阶的一个空闲内存块取出来，根据其起始地址和大小分割成两个新的小内存块，然后把大内存块free掉，小内存块放到(order-1)阶的链中。注意每阶对应的内存块都是用一个链表来链接的，这个链表的头指针就放在`bd_sys_binary`中。划分部分的伪代码如下：

        bd_sys_split(order):
            if bd_sys_binary[order] != NULL:
                start_va = bd_sys_binary[order]->va;
                size = bd_sys_binary[order]->size;
                new_blk_1 = bd_sys_alloc(va, size<<1);
                new_blk_2 = bd_sys_alloc(va+(size<<1), size<<1);
                bd_sys_append(order-1, new_blk_1, 0);
                bd_sys_append(order-1, new_blk_2, 0);
                bd_sys_free(bd_sys_binary[order]);
            else:
                try to split order+1 blocks
                if (success)
                    and try to split myself again
                
    从伪代码看出，如果这次尝试分配失败，即当前order没有空闲块，那么会递归调用更高一层的split，上层成功之后再尝试分配自己这层。
    
* **合并**：注意到上面的伪代码中有一个`bd_sys_append`函数，这个函数主要的功能是把一个内存块放置到指定阶的链表中，同时，如果在最后一个参数被标记为1，那么放置进去的时候还需要看看能不能跟自己的邻居块合并。如果可以合并的话，执行这样几步操作：
    1. 首先把两个相邻的内存块释放掉，然后计算出新的、上一阶内存块的大小和起始地址。
    2. 根据上一阶的起始地址和大小，可以从free list中拿一个MemoryBlock出来，把这个新算出来的起始地址和大小赋给这个块
    3. 把这个块同样用`bd_sys_append`传给上一阶
    根据这个算法，其实`bd_sys_append`也是递归调用的，当新的块到达上层之后，也会一样检查是否有邻居可以合并。
    
###系统调用接口

除了一开始的创建伙伴系统的函数`bd_sys_start`之外，另外两个函数也是作为系统调用的：

1. `bd_sys_alloc_blocks`：从伙伴系统中分配一个指定大小的块(结果是上取整到2的n次幂)
2. `bd_sys_free_blocks`：在伙伴系统中分配一个新的从指定`va`开始的，大小为`size`的块，然后放到指定的order中(order是根据`size`计算的)。

###正确性检验

在`bdsystest.c`中有一个很简单的执行正确性验证的程序。


