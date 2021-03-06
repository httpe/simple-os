#ifndef _TIME_H
#define _TIME_H

#include <stdint.h>

// From Linux struct_tm.h
/* ISO C `broken-down time' structure.  */
typedef struct date_time
{
  int tm_sec;			/* Seconds.	[0-60] (1 leap second) */
  int tm_min;			/* Minutes.	[0-59] */
  int tm_hour;			/* Hours.	[0-23] */
  int tm_mday;			/* Day.		[1-31] */
  int tm_mon;			/* Month.	[0-11] */
  int tm_year;			/* Year	- 1900.  */
  /* Below are not yet implemented */
  int tm_wday;			/* Day of week.	[0-6] */
  int tm_yday;			/* Days in year.[0-365]	*/
  int tm_isdst;			/* DST.		[-1/0/1]*/

  long int tm_gmtoff;		/* Seconds east of UTC.  */
  const char *tm_zone;		/* Timezone abbreviation.  */

} date_time;

typedef int64_t time_t;

date_time current_datetime();
time_t datetime2epoch(date_time* tim_p);

#endif