#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	struct sysinfo info;
	int fd, len;
	char *fmt =
		"uptime     \t%llu\n"
		"totalpages \t%u\n"
		"freepages  \t%u\n"
		"inblocks   \t%llu\n"
		"outblocks  \t%llu\n"
		"inpackets  \t%llu\n"
		"outpackets \t%llu\n"
		;
	char buf[64];

	sys_sysinfo(&info);
	printf(fmt, info.uptime,
	       info.totalpages, info.freepages,
	       info.inblocks, info.outblocks,
	       info.inpackets, info.outpackets);
}
