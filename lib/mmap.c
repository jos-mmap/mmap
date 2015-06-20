// implement mmap from user space

#include <inc/lib.h>


// these 2 values will be used in user space
// feel free to modify them if you want to add more space to
// the buddy system
#define MAX_BD_SYS_SHIFT 22
#define MAX_BD_SYS_SIZE (1<<(MAX_BD_SYS_SHIFT))

int bd_sys_size = 0; // haven't been initialized

//
// Custom page fault handler - it handles both page fault in mmap
// and normal COW.
//
void
mmap_pgfault(struct UTrapframe *utf) {
  void *addr = (void *) utf->utf_fault_va;
  void *roundaddr = ROUNDDOWN(addr, PGSIZE);
  uint32_t err = utf->utf_err;
  int r, i;
  struct mmap_metadata* md;

  pde_t pde = uvpd[PDX(addr)];
  pde_t pte = uvpt[PGNUM(addr)];
  if (!((pde & PTE_P) && (pte & PTE_P))) {
    int mmapmd_id = PGNUM(pte);
    md = &mmap_md[mmapmd_id];
    md->perm |= PTE_P;
    int curoffset = (uintptr_t)roundaddr - (uintptr_t)md->beginva;
    if ((r = fdmmap(md->fd, roundaddr, PGSIZE,
            curoffset, md->perm)) < 0) {
      panic("mmap fdmmap failed %e\n", r);
    }
  }
  if ((err & FEC_WR) != 0) { // write page fault
    if (!((pde & PTE_P) && (pte & PTE_P)) && (pte & PTE_COW)) {
      panic("mmap_pgfault: page should be mapped PTE_COW.");
    }
    for (i = 0 ; i < MAXMD ; ++i) {
      if (mmap_md[i].avail && mmap_md[i].beginva <= roundaddr &&
          mmap_md[i].beginva + mmap_md[i].length > roundaddr) {
        break;
      }
    }
    int newperm;
    if (i == MAXMD) {
      newperm = (uvpt[PGNUM(addr)] & PTE_SYSCALL & (~PTE_COW)) | PTE_W;
    } else {
      md = &mmap_md[i];
      newperm = md->perm | PTE_P;
    }
    if ((r = sys_page_alloc(0, (void*)PFTEMP,
                            newperm)) < 0) {
      panic("pgfault: error %e\n", r);
    }
    memcpy((PFTEMP), roundaddr, PGSIZE);
    if ((r = sys_page_map(0, (void*)PFTEMP,
                          0, roundaddr, newperm)) < 0) {
      panic("pgfault: error %e\n", r);
    }
    if ((r = sys_page_unmap(0, (void*)PFTEMP)) < 0)  {
      panic("pgfault: error %e\n", r);
    }
  }
}

void mmapmd_init() {
  int i;
  for (i = 0 ; i < MAXMD ; ++i) {
    mmap_md[i].avail = true;
  }
}

int mmapmd_alloc() {
  int i;
  for (i = 0 ; i < MAXMD ; ++i) {
    if (mmap_md[i].avail) {
      mmap_md[i].avail = false;
      return i;
    }
  }
  assert(i == MAXMD);
  return -E_NO_MMAPMD;
}

void mmapmd_destroy(int id) {
  mmap_md[id].avail = false;
}

bool mmapmd_checkid(int id) {
  return (id >= 0 && id < MAXMD);
}

//
// User-level mmap.
// This function is corresponding with POSIX.
// The page fault handler has been set in lib/libmain.c before
// calling umain.
// Register the region of mmaped address in a global data
// structure, mmap_metadata. Use buddy system to reserve a
// aligned consecutive address space for mmap.
// Returns: The begin address for mmaped area.
// It will panic on error.
//
void *mmap(void *addr, size_t length, int prot, int flags, int fdnum, off_t offset) {

  struct Fd* fd;
  struct mmap_metadata* md;
  int mmapmd_id;
  int r;

  cprintf("enter mmap: addr %p, length %u, prot %d, "
      "flags %d, fdnum %d, offset %u\n", addr, length, prot,
      flags, fdnum, offset);

    // first of all, here to start the mmap system
    // (or I'd rather say, the buddy system)
  if ((bd_sys_size = sys_bd_sys_start(UTEXT+PTSIZE, MAX_BD_SYS_SIZE)) < 0) {
    // if the return value is less than 0, it means:
    // 1. there's no enough space for a buddy system with
    // size MAX_BD_SIZE
    // 2. others I don't know
    // note that this function will return the size the bd
    // system really allocated
    panic("buddy system initialization failed.");
  }

  // sanity check for offset, which must be a multiple of
  // the page size.
  if ((offset % PGSIZE) != 0) {
    panic("mmap offset not aligned.");
  }

  // sanity check for prot, which must not conflict with the
  // open mode of the file.
  // TODO PROT_EXEC PROT_WRITE PROT_NONE
  // TODO PRIVATE & PROT_WRITE ?
  if ((r = fd_lookup(fdnum, &fd)) < 0) {
    panic("mmap fd lookup failed: %e,", r);
  }
  if ((prot & PROT_WRITE) && (fd->fd_omode & O_RDONLY)) {
    panic("mmap prot invalid: prot %d", prot);
  }

    int perm = PTE_U;
  if (flags & MAP_SHARED) {
    perm |= PTE_SHARE;
    if (prot & PROT_WRITE) {
      perm |= PTE_W;
    }
  } else {
    if (prot & PROT_WRITE) {
      perm |= PTE_COW;
    }
  }

  if ((mmapmd_id = mmapmd_alloc()) < 0) {
    panic("mmap alloc_mmapmd failed %e\n", mmapmd_id);
    exit();
  }

  void* retva = sys_page_reserve(addr, length, perm, mmapmd_id);
  if ((uintptr_t)retva % PGSIZE != 0) {
    mmapmd_destroy(mmapmd_id);
    panic("retva not aligned");
  }

  md = &mmap_md[mmapmd_id];
  md->beginva = retva;
  md->endva = ROUNDUP(retva + length, PGSIZE);
  md->length = length;
  md->fd = fdnum;
  md->prot = prot;
  md->flags = flags;
  md->offset = offset;
  md->perm = perm;

  return retva;
}

//
// User-level munmap.
// Only support unmap the region combined with regions
// allocated in one mmap
// On success, munmap() returns 0.
// On failure, it returns errno.
//
int munmap(void *addr, size_t length) {
  cprintf("enter munmap: addr %p, length %u\n", addr, length);

  if (((uintptr_t)addr % PGSIZE) != 0) {
    panic("munmap addr not aligned.");
  }

  void *unmap_begin = addr;
  void *unmap_end   = ROUNDUP(addr + length, PGSIZE);

  int i = 0;
  void *map_begin, *map_end;
  void *va;
  for (i = 0 ; i < MAXMD ; ++i) {
    if (!mmap_md[i].avail) {
      map_begin = mmap_md[i].beginva;
      map_end   = mmap_md[i].endva;
      cprintf("map_begin: %p,\tmap_end %p\n", map_begin, map_end);
      cprintf("unmap_begin: %p,\tunmap_end %p\n", unmap_begin, unmap_end);
      if (map_begin == unmap_begin &&
          map_end == unmap_end) {
        for (va = map_begin ; va < map_end ; va += PGSIZE) {
          sys_page_unmap(0, va);
        }
        cprintf("%p\n", unmap_begin);
        sys_bd_sys_free_blocks((uintptr_t)unmap_begin,
            (int)(unmap_end - unmap_begin));
        mmapmd_destroy(i);
      } else if (!((map_begin >= unmap_end) ||
                   (unmap_begin >= map_end))) {
          panic("only support unmap the region allocated in one map.");
      }
    }
  }
  return 0;
}
