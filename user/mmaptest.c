// test mmap
#include <inc/lib.h>

static void
mmap_pgfault_s(struct UTrapframe *utf) {

	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

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
}

void
umain(int argc, char **argv)
{

//   set_pgfault_handler(mmap_pgfault); //?
  int r, i, j, fd;
  char buf[512];

  cprintf("start creating file\n");
	if ((fd = open("/testmmap", O_WRONLY|O_CREAT)) < 0)
		panic("creat /testmmap: %e", fd);
	memset(buf, 0, sizeof(buf));
	for (i = 0; i < (NDIRECT*3)*BLKSIZE; i += sizeof(buf)) {
		*(int*)buf = i;
		if ((r = write(fd, buf, sizeof(buf))) < 0)
			panic("write /testmmap@%d: %e", i, r);
	}
	close(fd);
  cprintf("finish creating file\n");

	if ((fd = open("/testmmap", O_WRONLY|O_CREAT)) < 0)
		panic("creat /testmmap: %e", fd);
  cprintf("fd: %d\n", fd);
  int* addr_private = (int*)mmap((void*)0xeebfd000 - PGSIZE, PGSIZE, PROT_WRITE, MAP_PRIVATE, fd, 0);
  for (i = 0 ; i < PGSIZE/4 ; ++i) {
    cprintf("addr_private %d\n", addr_private[i]);
    addr_private[i] = 0x01010101;
  }
  close(fd);

  cprintf("(mmap private) start checking file\n");
	if ((fd = open("/testmmap", O_RDONLY)) < 0)
		panic("open /testmmap: %e", fd);
  if ((r = readn(fd, buf, sizeof(buf))) < 0)
    panic("read /testmmap@%d: %e", i, r);
  if (r != sizeof(buf))
    panic("read /testmmap from %d returned %d < %d bytes",
          0, r, sizeof(buf));
  for (j = 0 ; j < sizeof(buf) ; ++j) {
    cprintf("%d\n", buf[j]);
  }
	close(fd);

  cprintf("(mmap private) finish checking file\n");

	if ((fd = open("/testmmap", O_WRONLY|O_CREAT)) < 0)
		panic("creat /testmmap: %e", fd);
  cprintf("fd: %d\n", fd);
  int* addr_shared = (int*)mmap((void*)0xeebfd000 - 2*PGSIZE, PGSIZE, PROT_WRITE, MAP_SHARED, fd, 0);
  for (i = 0 ; i < PGSIZE/4 ; ++i) {
    cprintf("addr_shared %d\n", addr_shared[i]);
    addr_shared[i] = 0x01010101;
  }
  close(fd);

  cprintf("(mmap shared) start checking file\n");
	if ((fd = open("/testmmap", O_RDONLY)) < 0)
		panic("open /testmmap: %e", fd);
  if ((r = readn(fd, buf, sizeof(buf))) < 0)
    panic("read /testmmap@%d: %e", i, r);
  if (r != sizeof(buf))
    panic("read /testmmap from %d returned %d < %d bytes",
          0, r, sizeof(buf));
  for (j = 0 ; j < sizeof(buf) ; ++j) {
    cprintf("%d\n", buf[j]);
  }
	close(fd);
  cprintf("(mmap shared) finish checking file\n");

}
