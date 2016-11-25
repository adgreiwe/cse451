#include <inc/assert.h>
#include <inc/error.h>
#include <inc/string.h>

#include <kern/env.h>
#include <kern/nvme.h>
#include <kern/nvmereg.h>
#include <kern/pci.h>
#include <kern/pcireg.h>
#include <kern/pmap.h>

// queue sizes
#define ADMINQ_SIZE	8
#define IOQ_SIZE	8

struct nvme_queue {
	uint16_t id;
	size_t size;

	// submission queue
	void *sq_va;
	volatile void *sq_tdbl;
	uint32_t sq_tail;

	// completion queue
	void *cq_va;
	volatile void *cq_hdbl;
	uint32_t cq_head;
	uint16_t cq_phase;
};

// queues
static struct nvme_queue adminq, ioq;

static void
nvme_queue_init(struct nvme_queue *q, volatile void *base, uint16_t id, size_t size, size_t dstrd)
{
	struct PageInfo *p;

	memset(q, 0, sizeof(*q));
	q->id = id;
	q->size = size;

	p = page_alloc(ALLOC_ZERO);
	assert(p);
	p->pp_ref++;
	q->sq_va = page2kva(p);
	q->sq_tdbl = base + NVME_SQTDBL(id, dstrd);

	p = page_alloc(ALLOC_ZERO);
	assert(p);
	p->pp_ref++;
	q->cq_va = page2kva(p);
	q->cq_hdbl = base + NVME_CQHDBL(id, dstrd);
}

static int
nvme_queue_submit(struct nvme_queue *q, void *cmd)
{
	struct nvme_sqe *sqe = q->sq_va;
	volatile struct nvme_cqe *cqe = q->cq_va;

	sqe += q->sq_tail;
	cqe += q->cq_head;

	// Copy to SQ
	memcpy(sqe, cmd, sizeof(struct nvme_sqe));

	// Bump the SQ tail pointer
	++q->sq_tail;
	if (q->sq_tail == q->size)
		q->sq_tail = 0;

	// Ring the SQ doorbell
	mmio_write32(q->sq_tdbl, q->sq_tail);

	// Wait for CQ
	while ((cqe->flags & NVME_CQE_PHASE) == q->cq_phase)
		;
	assert(NVME_CQE_SC(cqe->flags) == NVME_CQE_SC_SUCCESS);

	// Bump the CQ head pointer
	++q->cq_head;
	if (q->cq_head == q->size) {
		q->cq_head = 0;
		q->cq_phase ^= NVME_CQE_PHASE;
	}

	// Ring the CQ doorbell
	mmio_write32(q->cq_hdbl, q->cq_head);

	return 0;
}

static void
nvme_queue_create(struct nvme_queue *q)
{
	struct nvme_sqe_q cmd_add_iocq = {
		.opcode = NVM_ADMIN_ADD_IOCQ,
		.prp1 = PADDR(q->cq_va),
		.qsize = q->size - 1,
		.qid = q->id,
		.qflags = NVM_SQE_Q_PC,
	};
	struct nvme_sqe_q cmd_add_iosq = {
		.opcode = NVM_ADMIN_ADD_IOSQ,
		.prp1 = PADDR(q->sq_va),
		.qsize = q->size - 1,
		.qid = q->id,
		.cqid = q->id,
		.qflags = NVM_SQE_Q_PC,
	};

	static_assert(sizeof(struct nvme_sqe_q) == sizeof(struct nvme_sqe));
	// Create CQ
	nvme_queue_submit(&adminq, &cmd_add_iocq);
	// Create SQ
	nvme_queue_submit(&adminq, &cmd_add_iosq);
}

int
nvme_attach(struct pci_func *pcif)
{
	volatile void *base;
	uint32_t cc, dstrd;
	uint64_t cap;

	if (PCI_INTERFACE(pcif->dev_class) != PCI_INTERFACE_NVM_NVME)
		return 0;
	pci_func_enable(pcif);
	// BAR 0: NVMe base address
	base = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

	cap = mmio_read64(base + NVME_CAP);
	assert(NVME_CAP_MPSMIN(cap) <= PGSHIFT);
	assert(NVME_CAP_MPSMAX(cap) >= PGSHIFT);
	dstrd = NVME_CAP_DSTRD(cap);

	cc = mmio_read32(base + NVME_CC);
	assert(!ISSET(cc, NVME_CC_EN));

	nvme_queue_init(&adminq, base, NVME_ADMIN_Q, ADMINQ_SIZE, dstrd);
	// Set admin queue sizes
	mmio_write32(base + NVME_AQA, NVME_AQA_ACQS(ADMINQ_SIZE) | NVME_AQA_ASQS(ADMINQ_SIZE));
	// Set admin queue addresses
	mmio_write64(base + NVME_ACQ, PADDR(adminq.cq_va));
	mmio_write64(base + NVME_ASQ, PADDR(adminq.sq_va));

	// Set control configuration
	CLR(cc, NVME_CC_IOCQES_MASK | NVME_CC_IOSQES_MASK |
		NVME_CC_AMS_MASK | NVME_CC_MPS_MASK | NVME_CC_CSS_MASK);
	SET(cc, NVME_CC_IOSQES(__builtin_ffs(64) - 1) | NVME_CC_IOCQES(__builtin_ffs(16) - 1));
	SET(cc, NVME_CC_AMS(NVME_CC_AMS_RR));
	SET(cc, NVME_CC_MPS(PGSHIFT));
	SET(cc, NVME_CC_CSS(NVME_CC_CSS_NVM));
	SET(cc, NVME_CC_EN);
	mmio_write32(base + NVME_CC, cc);

	// Wait until the controller is ready
	while (!ISSET(mmio_read32(base + NVME_CSTS), NVME_CSTS_RDY))
		;

	// Create I/O queues
	nvme_queue_init(&ioq, base, 1, IOQ_SIZE, dstrd);
	nvme_queue_create(&ioq);
	return 1;
}

static physaddr_t
va2pa(void *va)
{
	struct PageInfo *pp;

	pp = page_lookup(curenv->env_pgdir, va, NULL);
	assert(pp);
	return page2pa(pp) + PGOFF(va);
}

static int
nvme_rw(uint8_t opcode, uint64_t secno, void *buf, uint16_t nsecs)
{
	struct PageInfo *pp;
	struct nvme_sqe_io cmd = {
		.opcode = opcode,
		.nsid = 1,
		.slba = secno,
		.nlb = nsecs - 1,
		.entry.prp[0] = va2pa(buf),
	};

	static_assert(sizeof(struct nvme_sqe_io) == sizeof(struct nvme_sqe));
	if (!ioq.sq_va)
		return -E_INVAL;
	// buf must be page aligned
	if (PGOFF(buf))
		return -E_INVAL;
	// support one page for now
	if (nsecs > BLKSECTS)
		return -E_INVAL;

	nvme_queue_submit(&ioq, &cmd);
	return nsecs;
}

int
nvme_read(uint64_t secno, void *buf, uint16_t nsecs)
{
	return nvme_rw(NVM_CMD_READ, secno, buf, nsecs);
}

int
nvme_write(uint64_t secno, void *buf, uint16_t nsecs)
{
	return nvme_rw(NVM_CMD_WRITE, secno, buf, nsecs);
}
