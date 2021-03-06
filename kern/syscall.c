/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/batch.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/sysinfo.h>
// you added
#include <kern/nvme.h>

// Returns 0 in the case that they're invalid or positive number
// if they're valid as defined in comments of sys_page_alloc:
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
static int
valid_perms(int perm)
{
	int user_readable = PTE_U | PTE_P;
	return ((perm & user_readable) == user_readable &&
		((~PTE_SYSCALL) & perm) == 0);
}


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

	// this was deleted in lab5 but it breaks the lab4 tests when commented out
	if (e == curenv) {
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	} else {
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	}

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
	if (!valid_perms(perm) || 
	    // now check va
	    va >= (void *) UTOP || 
	    (size_t) va % PGSIZE != 0) {
		return -E_INVAL;
	}
	// valid perm and va params
	
	struct Env *e;
	if (envid2env(envid, &e, 1) < 0) {
		return -E_BAD_ENV;
	}
	struct PageInfo *new_page = page_alloc(ALLOC_ZERO);
	if (page_insert(e->env_pgdir, new_page, va, perm) < 0) {
		page_free(new_page);
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
	
	if (srcva >= (void *) UTOP || (size_t) srcva % PGSIZE != 0 ||
	    dstva >= (void *) UTOP || (size_t) dstva % PGSIZE != 0 || 
	    !valid_perms(perm) || page == NULL ||
	    (!(*src_entry & PTE_W) && (perm & PTE_W))) {
		return -E_INVAL;
	}
	
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

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
	struct Env *new_env;
	int result = env_alloc(&new_env, curenv->env_id);
	if (result == 0) {
		new_env->env_status = ENV_NOT_RUNNABLE;
		new_env->env_tf = curenv->env_tf;
		new_env->env_tf.tf_regs.reg_eax = 0;
		return new_env->env_id;
	} else {
		return result;
	}
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
	if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE) {
		return -E_INVAL;
	}
	struct Env *e;
	if (envid2env(envid, &e, 1) < 0) {
		return -E_BAD_ENV;
	}

	e->env_status = status;
	return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0) {
		return r;
	}

	e->env_tf = *tf;
	return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env *e;
	if (envid2env(envid, &e, 1) < 0) {
		return -E_BAD_ENV;
	}

	e->env_pgfault_upcall = func;
	return 0;
}

// Return the current system information.
static int
sys_sysinfo(struct sysinfo *info)
{
	// LAB 4: Your code here.
	return sysinfo(info);
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	struct Env *dst_e;
	if (envid2env(envid, &dst_e, 0) < 0) {
		return -E_BAD_ENV;
	}

	if (!dst_e->env_ipc_recving) {
		return -E_IPC_NOT_RECV;
	}

	pte_t *src_entry;
	struct PageInfo *src_pg;
	if (srcva < (void *) UTOP) {
		src_pg = page_lookup(curenv->env_pgdir, srcva, &src_entry);
		if ((uintptr_t) srcva % PGSIZE != 0 || !valid_perms(perm) ||
				!src_pg || ((perm & PTE_W) && !(*src_entry & PTE_W))) {
			return -E_INVAL;
		}
		if (dst_e->env_ipc_dstva < (void *) UTOP) {
			if (page_insert(dst_e->env_pgdir, src_pg, dst_e->env_ipc_dstva, perm) < 0) {
				return -E_NO_MEM;
			}
			dst_e->env_ipc_perm = perm;
		}
	}

	// mapping was successful or no mapping in ipc was attempted
	dst_e->env_ipc_recving = 0;
	dst_e->env_ipc_from = curenv->env_id;
	dst_e->env_ipc_value = value;
	dst_e->env_status = ENV_RUNNABLE;

	// this part was not immediately obv to you... it's cause we did sched_yield so when
	// we start running this env again the eax register will hold the pseudo return value
	dst_e->env_tf.tf_regs.reg_eax = 0;

	return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	if (dstva < (void *) UTOP && (uintptr_t) dstva % PGSIZE != 0) {
		return -E_INVAL;
	}
	// dstva is either page-aligned and below UTOP or dstva is not above UTOP
	curenv->env_ipc_recving = 1;
	curenv->env_ipc_dstva = dstva;
	curenv->env_status = ENV_NOT_RUNNABLE;
	sched_yield();
	return 0;
}

static int
sys_blk_write(uint32_t secno, void *buf, size_t nsecs)
{
	// LAB 5: Your code here.
	// Check that the user has permission for buf.
	int r;
	if ((r = user_mem_check(curenv, buf, BLKSIZE, PTE_U)) == 0) {
		return nvme_write((uint64_t) secno, buf, (uint16_t) nsecs);
	}
	return r;
}

static int
sys_blk_read(uint32_t secno, void *buf, size_t nsecs)
{
	// LAB 5: Your code here.
	// Check that the user has permission for buf.
	int r;
	if ((r = user_mem_check(curenv, buf, BLKSIZE, PTE_U)) == 0) {
		return nvme_read((uint64_t) secno, buf, (uint16_t) nsecs);
	}
	return r;
}

static int
sys_batch(struct batch *sys_calls, uint32_t num_calls)
{
	for (int i = 0; i < num_calls; i++) {
		if (syscall(sys_calls[i].sys_no, sys_calls[i].p1, sys_calls[i].p2, 
			    sys_calls[i].p3, sys_calls[i].p4, sys_calls[i].p5) < 0) {
			panic("sys_batch: failed\n");
		}
	}
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

	case SYS_yield :
		// shift running env on processor
		sched_yield();
		return 0;

	case SYS_exofork :
		return sys_exofork();

	case SYS_env_set_status :
		// set env corresponding with envid in a1 to have status held in a2
		return sys_env_set_status(a1, (int) a2);

	case SYS_env_set_pgfault_upcall : 
		// set page fault upcall entry point (a2) for env of envid a1
		return sys_env_set_pgfault_upcall(a1, (void *) a2);

	case SYS_sysinfo :
		// get system info from struct sysinfo pointer stored in a1
		return sys_sysinfo((struct sysinfo *) a1);

	case SYS_ipc_try_send : 
		// try sending to env corresponding to envid in a1 the value in
		// a2 and the mapping stored in a3 with perm bits a4
		return sys_ipc_try_send(a1, a2, (void *) a3, (int) a4);

	case SYS_ipc_recv :
		// block for ipc with mapping for dstva stored in a1
		return sys_ipc_recv((void *) a1);

	case SYS_blk_write :
		// write to buffer in a2
		return sys_blk_write(a1, (void *) a2, a3);

	case SYS_blk_read :
		// read fron buffer in a2
		return sys_blk_read(a1, (void *) a2, a3);

	case SYS_env_set_trapframe :
		// set trapframe of env corresponding with envid a1 to one
		// at a2 
		return sys_env_set_trapframe(a1, (struct Trapframe *) a2);

	case SYS_batch:
		// batch a2 calls held in batch array a1
		return sys_batch((struct batch *) a1, a2);

	default:
		return -E_INVAL;
	}
}
