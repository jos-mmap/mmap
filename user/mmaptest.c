// test mmap
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
  cprintf("yeah\n");
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
  int* addr_private = (int*)mmap((void*)USTACKTOP - 2*PGSIZE, PGSIZE, PROT_WRITE, MAP_PRIVATE, fd, 0);
  for (i = 0 ; i < PGSIZE/4 ; ++i) {
    // Checks the file content is correct.
    if (i % 128 == 0) {
      assert(addr_private[i] == i*4);
    } else {
      assert(addr_private[i] == 0);
    }
    // Write to private address.
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
    // Checks the change to private address
    // doesn't change the file.
    assert(buf[j] == 0);
  }
	close(fd);

  cprintf("(mmap private) finish checking file\n");

	if ((fd = open("/testmmap", O_WRONLY|O_CREAT)) < 0)
		panic("creat /testmmap: %e", fd);
  cprintf("fd: %d\n", fd);
  int* addr_shared = (int*)mmap((void*)USTACKTOP - 3*PGSIZE, PGSIZE, PROT_WRITE, MAP_SHARED, fd, 0);
  for (i = 0 ; i < PGSIZE/4 ; ++i) {
    // Checks the file content is correct.
    if (i % 128 == 0) {
      assert(addr_shared[i] == i*4);
    } else {
      assert(addr_shared[i] == 0);
    }
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
    // Checks the change to private address
    // is synced with the filesystem correctly.
    assert(buf[j] == 1);
  }

	close(fd);

  cprintf("(mmap shared) finish checking file\n");

  cprintf("(munmap) test munmap\n");
  munmap(addr_shared, PGSIZE);
  munmap(addr_private, PGSIZE);

  // Checks for munmap. All pages are succesfully unmaped.
  assert((uvpt[PGNUM(addr_shared)] & PTE_P) == 0);
  assert(uvpt[PGNUM(addr_shared)] == 0);
  assert(uvpt[PGNUM(addr_private)] == 0);
  cprintf("(munmap) pass munmap test\n");

}
