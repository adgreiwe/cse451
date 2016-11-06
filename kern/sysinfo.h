#ifndef JOS_KERN_SYSINFO_H
#define JOS_KERN_SYSINFO_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/sysinfo.h>

extern uint64_t inblocks, outblocks;
extern uint64_t inpackets, outpackets;

void	time_tick(void);
int	sysinfo(struct sysinfo *info);

#endif	// !JOS_KERN_SYSINFO_H
