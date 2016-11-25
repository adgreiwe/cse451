#ifndef JOS_KERN_NVME_H
#define JOS_KERN_NVME_H

#include <inc/fs.h>
#include <kern/pci.h>

int nvme_attach(struct pci_func *pcif);
int nvme_write(uint64_t secno, void *buf, uint16_t nsecs);
int nvme_read(uint64_t secno, void *buf, uint16_t nsecs);

#endif	// JOS_KERN_NVME_H
