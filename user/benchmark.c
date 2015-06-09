#include <inc/lib.h>
#define INT_BUFFER_LEN 1024

int numbers[INT_BUFFER_LEN];

unsigned int getTimeInterval()
{
	static unsigned int t, nt;
	if (!t) {
		t = sys_time_msec();
		return 0;
	}
	t = nt;
	nt = sys_time_msec();
	return nt - t;
}

int doSomeComputation(int * numbers, size_t size)
{
	int answer = 0;
	size_t k;
		// totally arbitrary
	for(k = 0; k < size; k+= 2) {
		answer += numbers[k];
	}
	for(k = 1; k < size; k+= 2) {
		answer += numbers[k] * 2;
	}
	return answer;
}

void umain(int argc, char** argv)
{
	// print_fs_stat();

	int r, i, j, fd;
	int ans;
  char buf[512];

  int size = 1024 * 1024;

  getTimeInterval();

  cprintf("start creating a %d KB file\n", size / 1024);
	if ((fd = open("/testfile", O_WRONLY|O_CREAT)) < 0)
		panic("create /testfile: %e", fd);
	memset(buf, 0, sizeof(buf));
	for (i = 0; i < sizeof(buf); ++i)
		buf[i] = i % 3 + 1;
	for (i = 0; i < size; i += sizeof(buf)) 
		write(fd, buf, sizeof(buf));
	close(fd);
  cprintf("finish creating file in %u ms\n", getTimeInterval());

	// print_fs_stat();

	cprintf("Do some computation with __read__\n");
	fd = open("/testfile", O_RDONLY);
	ans = 0;
	for (i = 0; i < size; i += sizeof(numbers)) {
		readn(fd, &numbers[0], sizeof(numbers));
		ans += doSomeComputation(numbers, INT_BUFFER_LEN);
	}
	close(fd);
	// print_fs_stat();
	cprintf("test __read__ finished, sum %d, in %u ms\n\n", ans, getTimeInterval());


	cprintf("Do some computation with __mmap__\n");
	fd = open("/testfile", O_RDONLY);
	int * addr = (int *)mmap((void *)0x10000000, PGSIZE, PROT_WRITE, MAP_PRIVATE, fd, 0);
	ans = 0;
	for (i = 0; i < size; i += sizeof(numbers)) {
		ans += doSomeComputation(addr + (i / sizeof(int)), INT_BUFFER_LEN);
	}
	close(fd);
	// print_fs_stat();
	cprintf("test __mmap__ finished, sum %d, in %u ms\n\n", ans, getTimeInterval());

	// if ((fd = open("/testmmap", O_WRONLY|O_CREAT)) < 0)
	// 	panic("creat /testmmap: %e", fd);
 //  cprintf("fd: %d\n", fd);
 //  int* addr_private = (int*)mmap((void*)0xeebfd000 - PGSIZE, PGSIZE, PROT_WRITE, MAP_PRIVATE, fd, 0);
 //  for (i = 0 ; i < PGSIZE/4 ; ++i) {
 //    cprintf("addr_private %d\n", addr_private[i]);
 //    addr_private[i] = 0x01010101;
 //  }
 //  close(fd);

 //  cprintf("(mmap private) start checking file\n");
	// if ((fd = open("/testmmap", O_RDONLY)) < 0)
	// 	panic("open /testmmap: %e", fd);
 //  if ((r = readn(fd, buf, sizeof(buf))) < 0)
 //    panic("read /testmmap@%d: %e", i, r);
 //  if (r != sizeof(buf))
 //    panic("read /testmmap from %d returned %d < %d bytes",
 //          0, r, sizeof(buf));
 //  for (j = 0 ; j < sizeof(buf) ; ++j) {
 //    cprintf("%d\n", buf[j]);
 //  }
	// close(fd);

 //  cprintf("(mmap private) finish checking file\n");

	// if ((fd = open("/testmmap", O_WRONLY|O_CREAT)) < 0)
	// 	panic("creat /testmmap: %e", fd);
 //  cprintf("fd: %d\n", fd);
 //  int* addr_shared = (int*)mmap((void*)0xeebfd000 - 2*PGSIZE, PGSIZE, PROT_WRITE, MAP_SHARED, fd, 0);
 //  for (i = 0 ; i < PGSIZE/4 ; ++i) {
 //    cprintf("addr_shared %d\n", addr_shared[i]);
 //    addr_shared[i] = 0x01010101;
 //  }
 //  close(fd);

 //  cprintf("(mmap shared) start checking file\n");
	// if ((fd = open("/testmmap", O_RDONLY)) < 0)
	// 	panic("open /testmmap: %e", fd);
 //  if ((r = readn(fd, buf, sizeof(buf))) < 0)
 //    panic("read /testmmap@%d: %e", i, r);
 //  if (r != sizeof(buf))
 //    panic("read /testmmap from %d returned %d < %d bytes",
 //          0, r, sizeof(buf));
 //  for (j = 0 ; j < sizeof(buf) ; ++j) {
 //    cprintf("%d\n", buf[j]);
 //  }
	// close(fd);

	// print_fs_stat();
}