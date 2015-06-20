jos-mmap实现介绍 (by 李天石)
========

### mmap POSIX

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
MAP_SHARED Share this mapping.  Updates to the mapping are visible to
           other processes that map this file, and are carried
           through to the underlying file.  (To precisely control
           when updates are carried through to the underlying file
           requires the use of msync(2).)

MAP_PRIVATE
           Create a private copy-on-write mapping.  Updates to the
           mapping are not visible to other processes mapping the
           same file, and are not carried through to the underlying
           file.  It is unspecified whether changes made to the file
           after the mmap() call are visible in the mapped region.

Memory mapped by mmap() is preserved across fork(2), with the same
       attributes.

A file is mapped in multiples of the page size.  For a file that is
not a multiple of the page size, the remaining memory is zeroed when
mapped, and writes to that region are not written out to the file.
The effect of changing the size of the underlying file of a mapping
on the pages that correspond to added or removed regions of the file
is unspecified.

int munmap(void *addr, size_t length);

### 可能的提升

扩展磁盘大小 （复杂程度，意义？）

在fs进程中建立从blockno到virtual address的映射

分配连续的一段虚拟地址(这个在github上找到的实现并没有做优化)
开启大页优化

### 参考实现

github上找到的jos-mmap
serve_mmap: 改变fs中va的perm
MAP_PRIVATE是copy on write
(MAP_SHARE是share)

发送ipc从fs中读出一页到内存中

然而我觉得改变fs中va的perm并不太科学,这样对于同一个文件就不能同时SHARE和PRIVATE了

### 第一版实现

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

TODO sys_page_reserve 保留一段空间（并map成合适的perm） （虚拟内存分配）

MMAP_PRIVATE的perm是 PTE_U | PTE_COW, MMAP_SHARE的perm是 PTE_U | PTE_SHARE | (根据fd的读写模式和prot选择是否有PTE_W)

这样就可以借助已有的机制实现mmap_private和mmap_share了。可写的页如果是MMAP_PRIVATE，则在被写的时候会触发COW pgfault_handler，自然地重新分配一个页。

调用ipc从文件系统中把这一段数据加载到内存当中,并返回va，在mmap的进程中重新map

(若SHARE，则直接用SHARE的权限map，否则用~PTE_W|PTE_COW的权限map)

ipc_send:

  fd,

  va,

  len,

  offset,

  perm,

ipc_recv:

  // Return 0 on success, <0 on error.

### 第二版实现

把将文件读入内存的过程移到pgfault_handler中

pagefault时发送一个ipc给fs，行为和之前一步一样 (已添加)
利用全局的数据结构mmap_metadata，在pagefault时判断是否是mmap，并获得对应页的perm。处理COW的pagefault时 如果MAP_PRIVATE且文件可写，
那么给权限位添加PTE_W 否则不添加。不去掉PTE_COW位。

TODO 认真地处理一下pagefault,以后会把两个pagefault放在同一个handler里面做一个dispatch (比如有了全局的mmap_metadata之后就可以判断这个地址是不是属于一个mmap，以及相应的权限)
