
#include <inc/lib.h>

#define MAX_BD_SYS_SHIFT 22
#define MAX_BD_SYS_SIZE (1<<(MAX_BD_SYS_SHIFT))

void umain(int argc, char *argv[]) {
	cprintf("hello world, this is buddy system test\n");

	int bd_sys_size = 0; // haven't been initialized
	// first of all, here to start the mmap system(or I'd rather say, the buddy system)
	if ((bd_sys_size = sys_bd_sys_start(UTEXT, MAX_BD_SYS_SIZE)) < 0) {
		// if the return value is less than 0, it means:
		// 1. there's no enough space for a buddy system with size MAX_BD_SIZE
		// 2. others I don't know
		// note that this function will return the size the bd system really allocated
		panic("buddy system initialization failed.");
	}
	cprintf("buddy system init succeed!\n");
	// here try to allocate a block
	int r;
	// this one will cause kernel panic
	//r = sys_bd_sys_alloc_blocks(UTEXT, MAX_BD_SYS_SIZE+1);
	r = sys_bd_sys_alloc_blocks(UTEXT, 4096); cprintf("allocated at position: 0x%08x\n", r);
	r = sys_bd_sys_alloc_blocks(UTEXT, 4096); cprintf("allocated at position: 0x%08x\n", r);
	r = sys_bd_sys_alloc_blocks(UTEXT, 4096); cprintf("allocated at position: 0x%08x\n", r);
	r = sys_bd_sys_alloc_blocks(UTEXT, 8192); cprintf("allocated at position: 0x%08x\n", r);
	r = sys_bd_sys_free_blocks(UTEXT, 4096); 
	r = sys_bd_sys_free_blocks(UTEXT+4096, 4096); 
}
