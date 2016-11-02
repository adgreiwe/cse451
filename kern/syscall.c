/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv, s, len, 0);
	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	if (e == curenv)
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	else
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 3: Your code here.
	int user_readable = PTE_U | PTE_P;
	if ((uint32_t) (perm | 0xfff) > 0xfff ||
	    (perm & user_readable) != user_readable ||
	    ((~PTE_SYSCALL) & perm) != 0 ||
	    // now check va
	    va >= (void *) UTOP || 
	    (size_t) va % PGSIZE != 0) {
		return -E_INVAL;
	}
	// valid perm and va params
	
	struct Env *e = 0;
	if (envid2env(envid, &e, 1) < 0) {
		return -E_BAD_ENV;
	}
	struct PageInfo *new_page = page_alloc(ALLOC_ZERO);
	if (page_insert(e->env_pgdir, new_page, va, perm) != 0) {
		page_remove(e->env_pgdir, va);
		return -E_NO_MEM;
	}
	return 0;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 3: Your code here.

	struct Env *src_e;
	struct Env *dst_e;
	if (envid2env(srcenvid, &src_e, 1) < 0 ||
	    envid2env(dstenvid, &dst_e, 1) < 0) {
		return -E_BAD_ENV;
	}

	pte_t *src_entry;
	struct PageInfo *page = page_lookup(src_e->env_pgdir, srcva, &src_entry);
	
	int user_readable = PTE_P | PTE_U;
	if (srcva >= (void *) UTOP || (size_t) srcva % PGSIZE != 0 ||
	    dstva >= (void *) UTOP || (size_t) dstva % PGSIZE != 0 || 
	    (uint32_t) (perm | 0xfff) > 0xfff ||
	    (perm & user_readable) != user_readable ||
	    ((~PTE_SYSCALL) & perm) != 0 || page == NULL || //USE page_lookup instead
	    (((*src_entry & PTE_W) & perm) == 0 && (perm & PTE_W))) {
		return -E_INVAL;
	}
	// necessary to check *(pte_t - PTX(va)) write vs perm too?
	
	if (page_insert(dst_e->env_pgdir, page, dstva, perm) < 0) {
		return -E_NO_MEM;
	}

	return 0;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 3: Your code here.
	struct Env *e;
	if (envid2env(envid, &e, 1) < 0) {
		return -E_BAD_ENV;
	}

	if (va >= (void *) UTOP || (size_t) va % PGSIZE != 0) {
		return -E_INVAL;
	}

	page_remove(e->env_pgdir, va);
	return 0;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

	switch (syscallno) {

	case SYS_cputs :
		// print string located at va a1 of length a2 to system console 
		sys_cputs((char *) a1, a2);
		return 0;

	case SYS_getenvid :
		// return envid_t that corresponds to curenv
		return sys_getenvid();

	case SYS_env_destroy :
		// destroy environment that corresponds to envid_t in a1
		return sys_env_destroy(a1);

	case SYS_page_alloc :
		// allocate page for environment that corresponds to envid_t in a1
		// page is at va a2 with perm bits a3
		return sys_page_alloc(a1, (char *) a2, (int) a3);

	case SYS_page_map :
		// map page at va a2 to va a4 with perm bits a5 where src/dest
		// envs correspond with envid_t in a1/a3, respectively 
		return sys_page_map(a1, (void *) a2, a3, (void *) a4, (int) a5);

	case SYS_page_unmap :
		// Unmap page at va a2 in environment that corresponds to envid_t a1
		return sys_page_unmap(a1, (void *) a2);

	default:
		return -E_INVAL;
	}
}
