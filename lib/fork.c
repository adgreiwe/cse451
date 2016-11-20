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
	// Check that fault is write and that PTE is PTE_COW
	if (!(err & FEC_WR) || !(uvpt[PGNUM(addr)] & PTE_COW)) {
		panic("pgault: failed");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.	
	// Allocate a new mapped at PFTEMP 
	if (sys_page_alloc(0, PFTEMP, PTE_W | PTE_U | PTE_P) < 0) {
		panic("pgault: failed");
	}
	// Copy data of old page aligned page to new page
	addr = ROUNDDOWN(addr, PGSIZE);
	memcpy(PFTEMP, addr, PGSIZE);
	// Move new page to old page's address
	if (sys_page_map(0, PFTEMP, 0, addr, PTE_W | PTE_U | PTE_P) < 0) {
		panic("pgault: failed");
	}
	// Unmap the temporary page
	if (sys_page_unmap(0, PFTEMP) < 0) {
		panic("pgault: failed");
	}
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
	// LAB 4: Your code here.
	// Set up virtual address
	void* addr = (void*) (pn * PGSIZE);
	// Set default permussions
	int perm = PTE_U | PTE_P;
	if (uvpt[pn] & (PTE_W | PTE_COW)) {
		// Mapping must be marked as copy-on-write
		perm |= PTE_COW;
		// Map page into target environment
		if (sys_page_map(0, addr, envid, addr, perm) < 0) {
			panic("duppage: failed");
		}
		// Remap page as copy-on-writeable
		if (sys_page_map(0, addr, 0, addr, perm) < 0) {
			panic("duppage: failed");
		}
	} else {
		// If not copy-on-writeable simply map into target environment
		if (sys_page_map(0, addr, envid, addr, perm) < 0) {
			panic("duppage: failed");
		}
	}
	return 0;
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
	// Set up page fault handler
	set_pgfault_handler(pgfault);
	// Create a child
	envid_t envid = sys_exofork();
	if (envid < 0) {
		panic("fork: failed");
	}		
	// In case this is child fix thisenv
	if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	// For pages between UTEXT and USTACKTOP call duppage
	for (uintptr_t va = UTEXT ; va < USTACKTOP; va += PGSIZE){
		if (uvpd[PDX(va)] & PTE_P && uvpt[PGNUM(va)] & PTE_U) {
			duppage(envid, PGNUM(va));
		}
	}
	// Allocate new page for child's user exception stack
	if (sys_page_alloc(envid, (void*) (UXSTACKTOP-PGSIZE), PTE_U | PTE_P | PTE_W)) {
		panic("fork: failed");
	}
	// Copy page fault handler to the child
	extern void _pgfault_upcall();
	if (sys_env_set_pgfault_upcall(envid, _pgfault_upcall) < 0) {
		panic("fork: failed");
	}
	// Set chuld status as runnable
	if (sys_env_set_status(envid, ENV_RUNNABLE) < 0) {
		panic("fork: failed");
	}
	// Return child's envid
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
