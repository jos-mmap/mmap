// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
  if ((err & FEC_WR) == 0) {
    cprintf("%08x\n", err);
    panic("pgfault: error is not FEC_WR.");
  }
  pde_t pde = uvpd[PDX(addr)];
  pde_t pte = uvpt[PGNUM(addr)];
  if (!((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P)
      && (uvpt[PGNUM(addr)] & PTE_COW))) {
    panic("pgfault: page should be mapped PTE_COW.");
  }

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
  void* tmpva = (void*) ROUNDDOWN((uintptr_t) addr, PGSIZE);
  int newperm = (uvpt[PGNUM(addr)] & PTE_SYSCALL & (~PTE_COW)) | PTE_W;
  if ((r = sys_page_alloc(0, (void*)PFTEMP,
                          newperm)) < 0) {
    panic("pgfault: error %e\n", r);
  }
  memcpy((PFTEMP), tmpva, PGSIZE);
  if ((r = sys_page_map(0, (void*)PFTEMP,
                        0, tmpva, newperm)) < 0) {
    panic("pgfault: error %e\n", r);
  }
  if ((r = sys_page_unmap(0, (void*)PFTEMP)) < 0)  {
    panic("pgfault: error %e\n", r);
  }
//	panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
  int iscow = ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) ? PTE_COW : 0;
  void* target_va = (void*)(pn*PGSIZE);
  if ((r = sys_page_map(0, target_va,
                        envid, target_va,
                        (uvpt[pn] & PTE_SYSCALL & (~PTE_W)) | iscow)) < 0) {
    return r;
  }
  // cprintf("map %x\n", *(int*)0xeebfdf08);
  if (iscow) {
    if ((r = sys_page_map(0, target_va,
                          0, target_va,
                          (uvpt[pn] & PTE_SYSCALL & (~PTE_W)) | iscow)) < 0) {
      return r;
    }
  }
  // cprintf("remap\n");
  return 0;
	panic("duppage not implemented");
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
  int r;
  set_pgfault_handler(pgfault); //?
  envid_t cid = sys_exofork();
  cprintf("lib/fork.c: cid: %08x\n", cid);
  if (cid == 0) {
    // cprintf("heyyyyyyyyyyyyyy\n");
    thisenv = &envs[ENVX(sys_getenvid())];
    // set_pgfault_handler(pgfault); //?
    return 0;
  }
  if (cid < 0) {
    return cid;
  }
  int i;
  for (i = 0 ; i < USTACKTOP ; i += PGSIZE) {
    if ((uvpd[PDX(i)] & PTE_P) && (uvpt[PGNUM(i)] & PTE_P) && (uvpt[PGNUM(i)] & PTE_U)) {
      duppage(cid, PGNUM(i));
    }
  }
  extern void _pgfault_upcall(void);
  if ((r = sys_env_set_pgfault_upcall(cid, _pgfault_upcall)) < 0) {
    panic("sys_env_set_pgfault_upcall: error %e", r);
  }
  if ((r = sys_page_alloc(cid, (void*)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) < 0) {
    panic("sys_env_set_pgfault_upcall: alloc uxstack error %e", r);
  }
  if ((r = sys_env_set_status(cid, ENV_RUNNABLE)) < 0) {
    panic("sys_env_set_status: error %e", r);
  }
  return cid;
	panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
