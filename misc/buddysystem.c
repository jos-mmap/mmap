
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define uintptr_t unsigned int

#define panic(x) printf(x)

#define MAX_BD_SHIFT 22
#define MAX_BD_SIZE (1<<(MAX_BD_SHIFT))
#define PGSHIFT 12
#define PGSIZE (1<<(PGSHIFT))

#define MAX_BD_ORDER (MAX_BD_SHIFT-PGSHIFT)
#define MAX_BD_NODES ((1<<(MAX_BD_ORDER))-1)

#define BD_START_VA 0

#define ORDER2SIZE(order) (1<<((order)+PGSHIFT))

uintptr_t bd_sys_start_va 	= BD_START_VA;
int bd_sys_size	 			= MAX_BD_SIZE;
int bd_sys_shift			= MAX_BD_SHIFT;
int bd_sys_max_order 		= MAX_BD_ORDER;

struct MemoryBlock {
	uintptr_t va;
	struct MemoryBlock *next;
} *bd_sys_binary[MAX_BD_ORDER+1];

struct MemoryBlock *bd_sys_new_memblock(uintptr_t va) {
	struct MemoryBlock *memblock = 
		(struct MemoryBlock *) malloc(sizeof(struct MemoryBlock));
	memblock->va = va;
	memblock->next = NULL;
	return memblock;
}

int bd_sys_append_block(int order, struct MemoryBlock *memblock) {
	printf("append order %d with va 0x%08x\n", order, memblock->va);
	struct MemoryBlock* freeptr = bd_sys_binary[order];
	if (freeptr == NULL)
		bd_sys_binary[order] = memblock;
	else {
		while (freeptr->next != NULL) 
			freeptr = freeptr->next;
		freeptr->next = memblock;
	}
	return 0;
}

void bd_sys_build(uintptr_t va, int size) {
	// this va should be aligned to PGSIZE
	if ((va & (PGSIZE-1))) {
		panic("va should be aligned to PGSIZE\n");
		exit(1);
	}
	// and the size should be equal to an order of 2
	// and aligned to PGSIZE
	for (bd_sys_shift = 0; bd_sys_shift <= MAX_BD_SHIFT && 
			!((size>>bd_sys_shift)&1); bd_sys_shift++);
	if (bd_sys_shift < PGSHIFT || (size>>(bd_sys_shift+1))) {
		printf("bd sys shift: %d %d\n", bd_sys_shift, size>>(bd_sys_shift+1));
		panic("bd sys size is not aligned to PGSIZE, or it's not an order of 2\n");
		exit(1);
	}

	bd_sys_max_order = bd_sys_shift-PGSHIFT;
	// initialize the linked list
	memset(bd_sys_binary, 0, sizeof(bd_sys_binary));

	// new memory block
	struct MemoryBlock *memblock = bd_sys_new_memblock(0);

	printf("max layer %d\n", bd_sys_max_order);
	bd_sys_binary[bd_sys_max_order] = memblock;
}

int bd_sys_get_order(int size) {
	int shift;
	if (size > bd_sys_size) {
		panic("request size larger than the bsys could handle.\n");
		exit(1);
	}
	for (shift = bd_sys_shift; !((size>>shift)&1) && shift >= 0; shift --)
		; // get the left most bit
	if (shift < PGSHIFT) {
		panic("size smaller than the smallest granularity");
		exit(1);
	}
	if (size&((1<<shift)-1)) 
		shift++;
	return shift - PGSHIFT;
}

int bd_sys_split(int order) {
	printf("split at order: %d\n", order);
	if (order == bd_sys_max_order+1) {
		// can't split more, just quit
		return -1;
	}
	if (order == 0) {
		panic("error order\n");
		exit(1);
	}
	int r;
	// do the split work
	// if there's a existing memblock in the current order
	struct MemoryBlock *memblock = bd_sys_binary[order];
	// just split it!
	if (memblock != NULL) {
		printf("split at va %x size %d\n", memblock->va, ORDER2SIZE(order));
		bd_sys_binary[order] = memblock->next; // freed
		struct MemoryBlock *l_block = bd_sys_new_memblock(memblock->va);
		struct MemoryBlock *r_block = bd_sys_new_memblock(memblock->va + ORDER2SIZE(order-1));
		printf("to 2 blocks 0x%08x | 0x%08x\n", l_block->va, r_block->va);
		bd_sys_append_block(order-1, l_block);
		bd_sys_append_block(order-1, r_block);
		return 0;
	}
	// or try the upper level
	if ((r = bd_sys_split(order + 1)) < 0) {
		return r;
	}
	// second pass
	memblock = bd_sys_binary[order];
	// just split it!
	if (memblock != NULL) {
		printf("split second try at va %x size %d\n", memblock->va, ORDER2SIZE(order));
		bd_sys_binary[order] = memblock->next; // freed
		struct MemoryBlock *l_block = bd_sys_new_memblock(memblock->va);
		struct MemoryBlock *r_block = bd_sys_new_memblock(memblock->va + ORDER2SIZE(order-1));
		printf("to 2 blocks 0x%08x | 0x%08x\n", l_block->va, r_block->va);
		bd_sys_append_block(order-1, l_block);
		bd_sys_append_block(order-1, r_block);
		return 0;
	}
	// I give up.
	return -1;
}


// this method return the space it could allocate, 
// at the same time, it will also modify the global binary tree
int bd_sys_alloc(int size, struct MemoryBlock** memblock) {
	int r;
	int order = bd_sys_get_order(size);
	printf("size %d -> order %d\n", size, order);
	if (bd_sys_binary[order] == NULL) {
		// now we should try to split the upper order
		if ((r = bd_sys_split(order+1)) < 0) 
			return -1;
	}
	// here we must have a block
	*memblock = bd_sys_binary[order];
	printf("could allocate a block with order %d: 0x%08x\n", order, (*memblock)->va);
	assert(*memblock != NULL);
	bd_sys_binary[order] = (*memblock)->next;
	return 0;
}

int bd_sys_free(uintptr_t va, int size) {
	int order = bd_sys_get_order(size);
	struct MemoryBlock *memblock = bd_sys_new_memblock(va);
	// append the freed block
	bd_sys_append_block(order, memblock);
	printf("freed an order %d block\n", order);
	return 0;
}

int main(int argc, char *argv[]) {
	bd_sys_build(0, MAX_BD_SIZE);
	printf("bd system size %d shift %d\n", bd_sys_size, bd_sys_shift);
	uintptr_t alloc_va;
	int alloc_size;
	int r;

	int big_req = MAX_BD_SIZE;
	int sml_req = PGSIZE;
	int mid_req = (PGSIZE<<1) + PGSIZE;

	// test get order works
	printf("order of %d is %d\n", big_req, bd_sys_get_order(big_req));
	printf("order of %d is %d\n", sml_req, bd_sys_get_order(sml_req));
	printf("order of %d is %d\n", mid_req, bd_sys_get_order(mid_req));
	
	struct MemoryBlock *big_mem;
	struct MemoryBlock *sml_mem;
	struct MemoryBlock *memblk1;
	if ((r = bd_sys_alloc(big_req, &big_mem)) < 0) { printf("can't find any memblock\n"); }
	if ((r = bd_sys_alloc(sml_req, &sml_mem)) < 0) { printf("can't find any memblock\n"); }
	if ((r = bd_sys_free(big_mem->va, big_req)) < 0) { printf("can't free memory\n"); }
	if ((r = bd_sys_alloc(sml_req, &sml_mem)) < 0) { printf("can't find any memblock\n"); }
	printf("allocated at va: %x\n", sml_mem->va);
	if ((r = bd_sys_alloc(sml_req, &sml_mem)) < 0) { printf("can't find any memblock\n"); }
	printf("allocated at va: %x\n", sml_mem->va);
	if ((r = bd_sys_alloc(sml_req, &sml_mem)) < 0) { printf("can't find any memblock\n"); }
	printf("allocated at va: %x\n", sml_mem->va);
	if ((r = bd_sys_alloc(big_req>>1, &big_mem)) < 0) { printf("can't find any memblock\n"); }
	printf("allocated at va: %x\n", big_mem->va);
	return 0;
}