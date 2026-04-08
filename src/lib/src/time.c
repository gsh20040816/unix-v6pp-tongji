#include "time.h"

static int DayOfWeekFromDaysSinceEpoch(unsigned int days)
{
	/* 1970-01-01 是星期四；SystemTime/tm 约定 Sunday=1。 */
	return (int)((days + 4) % 7) + 1;
}

static unsigned int DaysInMonthOfYear(int year, int month)
{
	unsigned int monthDays = daysInMonth[month];
	if ( month == 2 && isLeapYear(year) )
	{
		++monthDays;
	}
	return monthDays;
}

unsigned int gtime()
{
	int res;
	__asm__ volatile ("int $0x80":"=a"(res):"a"(13) );
	if ( res >= 0 )
		return res;
	return -1;
}

int stime(unsigned int seconds)
{
	int res;
	__asm__ volatile ("int $0x80":"=a"(res):"a"(25),"b"(seconds) );
	if ( res >= 0 )
		return res;
	return -1;
}

unsigned int daysInYear( int year )
{
	return isLeapYear(year) ? 366 : 365;
}

unsigned int mktime(struct tm* ptime)
{
	unsigned int timeInSeconds = 0;
	unsigned int days;
	int currentYear = 2000 + ptime->Year;	/* Year中只有年份后2位 */
	
	/* compute hours, minutes, seconds */
	timeInSeconds += ptime->Second;
	timeInSeconds += ptime->Minute * SECONDS_IN_MINUTE;
	timeInSeconds += ptime->Hour * SECONDS_IN_HOUR;
	
	/* compute days in current year */
	days = ptime->DayOfMonth - 1;
	days += daysBeforeMonth[ptime->Month];
	if (isLeapYear(currentYear) && ptime->Month >= 3 /* After February */)
		days++;

	/* compute days in previous years */
	int year;
	for (year = 1970; year < currentYear; year++)
	{
		days += daysInYear(year);
	}
	timeInSeconds += days * SECONDS_IN_DAY;
	
	ptime->DayOfWeek = DayOfWeekFromDaysSinceEpoch(days);
	
	return timeInSeconds;
}

static struct tm local_tm;
struct tm* localtime(unsigned int timeInSeconds)
{
	struct tm* ptime = &local_tm;
	memset(&local_tm, 0, sizeof(local_tm));
	
	/* compute days before today */
	unsigned int days = timeInSeconds / SECONDS_IN_DAY;
	ptime->DayOfWeek = DayOfWeekFromDaysSinceEpoch(days);
	int year = 1970;
	while(days >= 365)
	{
		if( days >= daysInYear(year) )
		{
			days -= daysInYear(year);
			year++;
		}
		else
		{
			break;
		}
	}
	ptime->Year = (year >= 2000) ? (year - 2000) : (year - 1900);
	
	/* Compute month in year & day of month */
	int month = 1;
	for( ; month <= 12; month++ )
	{
		unsigned int monthDays = DaysInMonthOfYear(year, month);
		if( days >= monthDays )
		{
			days -= monthDays;
		}
		else
		{
			break;
		}
	}
	ptime->Month = month;
	ptime->DayOfMonth = days + 1;
	
	/* Compute hour, minute, second */
	unsigned int secondsInToday = timeInSeconds % SECONDS_IN_DAY;
	ptime->Hour = secondsInToday / SECONDS_IN_HOUR;
	ptime->Minute = (secondsInToday % SECONDS_IN_HOUR) / SECONDS_IN_MINUTE;
	ptime->Second = secondsInToday % SECONDS_IN_MINUTE;
	
	return ptime;
}

static char asctime_buf[40];
char* asctime(struct tm* ptime)
{
	struct tm normalized;
	const char* month = monthName[0];
	const char* weekday = weekdayName[0];

	memset(asctime_buf, 0, sizeof(asctime_buf));

	/*
	 * asctime 不再直接信任调用方传入的 DayOfWeek。
	 * 这里基于年月日重新归一化一次，避免无效星期索引直接泄露为 "NOT Used"。
	 */
	normalized = *ptime;
	mktime(&normalized);
	if ( ptime->Month >= 1 && ptime->Month <= 12 )
	{
		month = monthName[ptime->Month];
	}
	if ( normalized.DayOfWeek >= 1 && normalized.DayOfWeek <= 7 )
	{
		weekday = weekdayName[normalized.DayOfWeek];
	}

	sprintf(asctime_buf, "%d-%s-%d %d:%d:%d(%s)", ptime->DayOfMonth, month,
			2000 + ptime->Year, ptime->Hour, ptime->Minute, ptime->Second, weekday);
			
	return asctime_buf;
}
