#include <inc/assert.h>
#include <inc/error.h>
#include <inc/string.h>

#include <kern/cpu.h>
#include <kern/sysinfo.h>
#include <kern/pmap.h>

#define NANOSECONDS_PER_TICK	(10 * NANOSECONDS_PER_MILLISECOND)

static uint64_t ticks = 0;
uint64_t inblocks, outblocks;
uint64_t inpackets, outpackets;

// This should be called once per timer interrupt.  A timer interrupt
// fires every 10 ms.
void
time_tick(void)
{
	++ticks;
	if (ticks > UINT64_MAX / NANOSECONDS_PER_TICK)
		panic("time_tick: time overflowed");
}

int
sysinfo(struct sysinfo *info)
{
	info->uptime = ticks * NANOSECONDS_PER_TICK;
	info->totalpages = npages;
	info->freepages = nfreepages;
	info->inblocks = inblocks;
	info->outblocks = outblocks;
	info->inpackets = inpackets;
	info->outpackets = outpackets;
	return 0;
}
