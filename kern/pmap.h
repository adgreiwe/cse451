/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_PMAP_H
#define JOS_KERN_PMAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/memlayout.h>
#include <inc/assert.h>
struct Env;

extern char bootstacktop[], bootstack[];

extern struct PageInfo *pages;
extern size_t npages, nfreepages;

extern pde_t *kern_pgdir;


/* This macro takes a kernel virtual address -- an address that points above
 * KERNBASE, where the machine's maximum 256MB of physical memory is mapped --
 * and returns the corresponding physical address.  It panics if you pass it a
 * non-kernel virtual address.
 */
#define PADDR(kva) _paddr(__FILE__, __LINE__, kva)

static inline physaddr_t
_paddr(const char *file, int line, void *kva)
{
	if ((uint32_t)kva < KERNBASE)
		_panic(file, line, "PADDR called with invalid kva %08lx", kva);
	return (physaddr_t)kva - KERNBASE;
}

/* This macro takes a physical address and returns the corresponding kernel
 * virtual address.  It panics if you pass an invalid physical address. */
#define KADDR(pa) _kaddr(__FILE__, __LINE__, pa)

static inline void*
_kaddr(const char *file, int line, physaddr_t pa)
{
	if (PGNUM(pa) >= npages)
		_panic(file, line, "KADDR called with invalid pa %08lx", pa);
	return (void *)(pa + KERNBASE);
}


enum {
	// For page_alloc, zero the returned physical page.
	ALLOC_ZERO = 1<<0,
};

void	mem_init(void);

void	page_init(void);
struct PageInfo *page_alloc(int alloc_flags);
void	page_free(struct PageInfo *pp);
int	page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm);
void	page_remove(pde_t *pgdir, void *va);
struct PageInfo *page_lookup(pde_t *pgdir, void *va, pte_t **pte_store);
void	page_decref(struct PageInfo *pp);

void	tlb_invalidate(pde_t *pgdir, void *va);

volatile void *	mmio_map_region(physaddr_t pa, size_t size);

static inline uint8_t
mmio_read8(volatile void *addr)
{
	return *(volatile uint8_t *)addr;
}

static inline uint16_t
mmio_read16(volatile void *addr)
{
	return *(volatile uint16_t *)addr;
}

static inline uint32_t
mmio_read32(volatile void *addr)
{
	return *(volatile uint32_t *)addr;
}

static inline uint64_t
mmio_read64(volatile void *addr)
{
	uint64_t lo, hi;

	lo = mmio_read32(addr);
	hi = mmio_read32(addr + 4);
	return lo + (hi << 32);
}

static inline void
mmio_write8(volatile void *addr, uint8_t v)
{
	*(volatile uint8_t *)addr = v;
}

static inline void
mmio_write16(volatile void *addr, uint16_t v)
{
	*(volatile uint16_t *)addr = v;
}

static inline void
mmio_write32(volatile void *addr, uint32_t v)
{
	*(volatile uint32_t *)addr = v;
}

static inline void
mmio_write64(volatile void *addr, uint64_t v)
{
	mmio_write32(addr, (uint32_t)v);
	mmio_write32(addr + 4, (uint32_t)(v >> 32));
}


int	user_mem_check(struct Env *env, const void *va, size_t len, int perm);
void	user_mem_assert(struct Env *env, const void *va, size_t len, int perm);

static inline physaddr_t
page2pa(struct PageInfo *pp)
{
	return (pp - pages) << PGSHIFT;
}

static inline struct PageInfo*
pa2page(physaddr_t pa)
{
	if (PGNUM(pa) >= npages)
		panic("pa2page called with invalid pa");
	return &pages[PGNUM(pa)];
}

static inline void*
page2kva(struct PageInfo *pp)
{
	return KADDR(page2pa(pp));
}

pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);

#endif /* !JOS_KERN_PMAP_H */
