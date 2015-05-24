// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
  { "backtrace", "Display the backtrace", mon_backtrace},
  { "showmappings", "Display physical page mappings", mon_showmappings},
  { "setperm", "Set the permission of mappings", mon_setperm},
  { "dumpvm", "Dump the memory content", mon_dumpva},
  { "dumppm", "Dump the memory content", mon_dumppa},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}
 
int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
  int ebp = read_ebp(), eip;
  int* args_ptr;
  struct Eipdebuginfo info;
  cprintf("Stack backtrace:\n");
  while(ebp) {
    cprintf("  ebp %08x", ebp);
    eip = *((int*)ebp + 1);
    cprintf("  eip %08x", eip);
    args_ptr = (int*)ebp + 2;
    cprintf(" args");
    int i = 0;
    while (i < 5) {
      cprintf(" %08x", *(args_ptr + i));
      ++i;
    }
    cprintf("\n");
    debuginfo_eip(eip, &info);
    cprintf("         %s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, eip - info.eip_fn_addr);
    ebp = *((int*)ebp);
  }
	return 0;
}

int xtoi(char* buf) {
  int res = 0;
  char *s = buf+2;
  while(*s) {
    res <<= 4;
    if (*s >= 'a') {
      res += *s-'a'+10;
    } else {
      res += *s-'0';
    }
    ++s;
  }
  return res;
  cprintf("%d\n", res);
}

int dtoi(char* buf) {
  int res = 0;
  char *s = buf;
  while(*s) {
    res *= 10;
    res += *s-'0';
    ++s;
  }
  return res;
  cprintf("%d\n", res);
}

void transfer_perm(int perm, char* perm_str) {
  if (perm & PTE_P) {
    strcat(perm_str, "P"); 
  } else {
    strcat(perm_str, "-");
  }
  if (perm & PTE_W) {
    strcat(perm_str, "W");
  } else {
    strcat(perm_str, "-");
  }
  if (perm & PTE_U) {
    strcat(perm_str, "U");
  } else {
    strcat(perm_str, "-");
  }
}

void print_mapping(uintptr_t va, uintptr_t pa, int perm) {
  char perm_str[100];
  perm_str[0] = 0;
  if (va != 0) {
    transfer_perm(perm, perm_str);
    cprintf("%08x\t%08x\t%s\n", va, pa, perm_str);
  } else {
    cprintf("%08x\t:not allocated.\n");
  }
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf) {
  if (argc < 2) {
    cprintf("Usage: showmappings [0xbegin_va] [0xend_va]\n");
  } else {
    cprintf("va\t\tpa\tperm\n");
    uintptr_t begin_va = xtoi(argv[1]);
    uintptr_t end_va = xtoi(argv[2]);
    uintptr_t va = begin_va;
    while(va <= end_va) {
      pte_t *pte_addr =
        pgdir_walk(kern_pgdir, (void*)va, false);
      if (pte_addr != NULL) {
        print_mapping(va, PTE_ADDR(*pte_addr), (*pte_addr) & 0xFFF);
      } else {
        print_mapping(va, 0, 0);
      }
      va += PGSIZE;
    }
  }
  return 0;
}

int
mon_setperm(int argc, char **argv, struct Trapframe *tf) {
  if (argc < 3) {
    cprintf("Usage: setperm [0xva] [0|1] [P|W|U]\n");
  } else {
    pte_t *pte =
      pgdir_walk(kern_pgdir, (void *)xtoi(argv[1]), false);
    int perm = 0;
    cprintf("%s\n", argv[2]);
    if (argv[3][0] == 'P') perm |= PTE_P;
    if (argv[3][1] == 'W') perm |= PTE_W;
    if (argv[3][2] == 'U') perm |= PTE_U;
    if (argv[2][0] == '0') {
      *pte = *pte & ~perm;
    } else {
      *pte = *pte | perm;
    }
  }
  return 0;
}

int
mon_dumpva(int argc, char **argv, struct Trapframe *tf) {
  if (argc < 2) {
    cprintf("Usage: dumpvm [0xva] [n]\n");
  } else {
    int va = xtoi(argv[1]);
    int i, n = dtoi(argv[2]);
    cprintf("va\tvalue\n");
    for (i = 0 ; i < n ; i += 4) {
      cprintf("%08x\t%08x\n", va, *(int*)va);
      va += 4;
    }
  }
  return 0;
}

int find_page(int pa) {
  int pa_aligned = PTE_ADDR(pa);
  int i;
  for (i = KERNBASE ; i < (1ll<<32) ; i += PGSIZE) {
    pte_t* pte_addr = pgdir_walk(kern_pgdir, (void *)i, false);
    if (pa_aligned == PTE_ADDR(*pte_addr)) {
      return i;
    }
  }
  return 0;
}

int
mon_dumppa(int argc, char **argv, struct Trapframe *tf) {
  if (argc < 2) {
    cprintf("Usage: dumppm [0xpa] [n]\n");
  } else {
    int pa = xtoi(argv[1]);
    int i, n = dtoi(argv[2]);
    cprintf("va\tvalue\n");
    int cnt = 0;
    while (cnt < n) {
      int va = find_page(pa);
      for (i = pa & 0xFFF ; cnt < n && i < 0x1000 ; i += 4, cnt += 4, pa += 4) {
        cprintf("%08x\t%08x\n", PTE_ADDR(pa)|i, *((int*)KADDR(PTE_ADDR(pa)|i)));
        cprintf("%08x\t%08x\n", PTE_ADDR(pa)|i, *((int*)(va|i)));
      }
    }
  }
  return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("%C01Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);
  // Lab1 Exercise 8-3
  {
    int x = 1, y = 3, z = 4;
    cprintf("x %d, y %x, z %d\n", x, y, z);
    cprintf("px %p\n", &x);
  }

  // Lab1 Exercise 8-4
  {
    unsigned int i = 0x00646c72;
    cprintf("H%x Wo%s", 57616, &i);
  }

  // Lab1 Exercise 8-5
  {
    cprintf("x=%d y=%d", 3);
  }

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
