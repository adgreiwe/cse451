#include <inc/lib.h>

nanoseconds_t
uptime(void)
{
	struct sysinfo info;

	sys_sysinfo(&info);
	return info.uptime;
}

void
nanosleep(nanoseconds_t nanoseconds)
{
	nanoseconds_t now, end;

	now = uptime();
	end = now + nanoseconds;
	if (end < now)
		panic("nanosleep: wrap");

	while (uptime() < end)
		sys_yield();
}

void
sleep(seconds_t seconds)
{
	nanosleep(seconds * NANOSECONDS_PER_SECOND);
}

static bool isleap(int year)
{
	return (year % 4 == 0) && !((year % 100 == 0) && (year % 400 != 0));
}

static int days_per_year(int year)
{
	return isleap(year) ? 366 : 365;
}

static int days_per_month(int year, int month)
{
	int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	if (isleap(year) && month == 1)
		return 29;
	return days[month];
}

// ignore leap seconds
void epoch_to_tm(seconds_t epoch, struct tm *tm)
{
	int year, month, days, dsec;

	days = epoch / 86400;
	dsec = epoch % 86400;

	tm->tm_sec = dsec % 60;
	tm->tm_min = (dsec % 3600) / 60;
	tm->tm_hour = dsec / 3600;
	tm->tm_wday = (days + 4) % 7;

	for (year = 1970; days >= days_per_year(year); ++year)
		days -= days_per_year(year);
	tm->tm_year = year - 1900;
	tm->tm_yday = days;

	for (month = 0; days >= days_per_month(year, month); ++month)
		days -= days_per_month(year, month);
	tm->tm_mon = month;
	tm->tm_mday = days + 1;
}
