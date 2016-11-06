#ifndef JOS_INC_SYSINFO_H
#define JOS_INC_SYSINFO_H

#include <inc/time.h>
#include <inc/types.h>

struct sysinfo {
	nanoseconds_t uptime;
	size_t totalpages, freepages;
	uint64_t inblocks, outblocks;
	uint64_t inpackets, outpackets;
};

#endif	// !JOS_INC_SYSINFO_H
