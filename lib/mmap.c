// implement mmap from user space

#include <inc/lib.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fdnum, off_t offset) {

  struct Fd* fd;
  int r;

  // sanity check for offset, which must be a multiple of the page size.
  if ((offset % PGSIZE) != 0) return (void *)-E_INVAL;

  // sanity check for prot, which must not conflict with the open mode of the file.
  // TODO PROT_EXEC PROT_WRITE PROT_NONE
  // TODO PRIVATE & PROT_WRITE ?
  if ((r = fd_lookup(fdnum, &fd)) < 0) {
    cprintf("mmap error: %e, addr %p, length %u, prot %d, flags %d, fdnum %d, offset %u\n", r, addr, length, prot, flags, fdnum, offset);
    exit();
  }
  cprintf("mmap fdnum %d fd %p\n", fdnum, fd);
  if ((prot & PROT_WRITE) && (fd->fd_omode & O_RDONLY)) {
    cprintf("mmap error: %e, addr %p, length %u, prot %d, flags %d, fdnum %d, offset %u\n", -E_INVAL, addr, length, prot, flags, fdnum, offset);
    exit();
  }

  // temporarily have PTE_P set
  int perm = PTE_P | PTE_U;
  if (flags & MAP_SHARED) {
    perm |= PTE_SHARE;
    if (prot & PROT_WRITE) {
      perm |= PTE_W;
    }
  } else {
    // TODO handle write privilege in page fault handler.
    // currently the default decision is to add PTE_W on COW pagefault, so
    // only write allowed situation can be correctly handled.
    perm |= PTE_COW;
  }
  cprintf("mmap perm %08x\n", perm);
  // TODO sys_page_reserve(void* addr, size_t len, int perm)
  void* retva = sys_page_reserve(addr, length, perm);
  cprintf("mmap retva %p\n", retva);
  assert((uintptr_t)retva % PGSIZE == 0);
  cprintf("mmap envid %08x\n", thisenv->env_id);
  if ((r = fdmmap(fdnum, retva, length, offset, perm)) < 0) {
    cprintf("mmap error: %e, addr %p, length %u, prot %d, flags %d, fdnum %d, offset %u\n", r, addr, length, prot, flags, fdnum, offset);
    exit();
  }
  return retva;
}

