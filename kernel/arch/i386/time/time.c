#include <kernel/time.h>
#include <arch/i386/kernel/port_io.h>
#include <kernel/timer.h>
#include <kernel/lock.h>
#include <string.h>
#include <common.h>

// Sourced from Newlib's mktime.c

#define _SEC_IN_MINUTE 60L
#define _SEC_IN_HOUR 3600L
#define _SEC_IN_DAY 86400L

static const int _DAYS_BEFORE_MONTH[12] =
{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

#define _ISLEAP(y) (((y) % 4) == 0 && (((y) % 100) != 0 || (((y)+1900) % 400) == 0))
#define _DAYS_IN_YEAR(year) (_ISLEAP(year) ? 366 : 365)


// sourced from https://wiki.osdev.org/CMOS#Getting_Current_Date_and_Time_from_RTC

#define CURRENT_YEAR      2021                    // Change this each year!

static int century_register = 0x00;                      // Set by ACPI table parsing code if possible

static struct {
    uchar second;
    uchar minute;
    uchar hour;
    uchar day;
    uchar month;
    uint year;
    yield_lock lk;
} curr;

enum {
    PORT_CMOS_ADDRESS = 0x70,
    PORT_CMOS_DATA = 0x71
};

static int get_update_in_progress_flag() {
    outb(PORT_CMOS_ADDRESS, 0x0A);
    return (inb(PORT_CMOS_DATA) & 0x80);
}

static uchar get_RTC_register(int reg) {
    outb(PORT_CMOS_ADDRESS, reg);
    return inb(PORT_CMOS_DATA);
}

static void read_rtc() {
    acquire(&curr.lk);

    uchar century;
    uchar last_second;
    uchar last_minute;
    uchar last_hour;
    uchar last_day;
    uchar last_month;
    uchar last_year;
    uchar last_century;
    uchar registerB;

    // Note: This uses the "read registers until you get the same values twice in a row" technique
    //     to avoid getting dodgy/inconsistent values due to RTC updates

    while (get_update_in_progress_flag());            // Make sure an update isn't in progress
    curr.second = get_RTC_register(0x00);
    curr.minute = get_RTC_register(0x02);
    curr.hour = get_RTC_register(0x04);
    curr.day = get_RTC_register(0x07);
    curr.month = get_RTC_register(0x08);
    curr.year = get_RTC_register(0x09);
    if (century_register != 0) {
        century = get_RTC_register(century_register);
    }

    do {
        last_second = curr.second;
        last_minute = curr.minute;
        last_hour = curr.hour;
        last_day = curr.day;
        last_month = curr.month;
        last_year = curr.year;
        last_century = century;

        while (get_update_in_progress_flag());         // Make sure an update isn't in progress
        curr.second = get_RTC_register(0x00);
        curr.minute = get_RTC_register(0x02);
        curr.hour = get_RTC_register(0x04);
        curr.day = get_RTC_register(0x07);
        curr.month = get_RTC_register(0x08);
        curr.year = get_RTC_register(0x09);
        if (century_register != 0) {
            century = get_RTC_register(century_register);
        }
    } while ((last_second != curr.second) || (last_minute != curr.minute) || (last_hour != curr.hour) ||
        (last_day != curr.day) || (last_month != curr.month) || (last_year != curr.year) ||
        (last_century != century));

    registerB = get_RTC_register(0x0B);

    // Convert BCD to binary values if necessary

    if (!(registerB & 0x04)) {
        curr.second = (curr.second & 0x0F) + ((curr.second / 16) * 10);
        curr.minute = (curr.minute & 0x0F) + ((curr.minute / 16) * 10);
        curr.hour = ((curr.hour & 0x0F) + (((curr.hour & 0x70) / 16) * 10)) | (curr.hour & 0x80);
        curr.day = (curr.day & 0x0F) + ((curr.day / 16) * 10);
        curr.month = (curr.month & 0x0F) + ((curr.month / 16) * 10);
        curr.year = (curr.year & 0x0F) + ((curr.year / 16) * 10);
        if (century_register != 0) {
            century = (century & 0x0F) + ((century / 16) * 10);
        }
    }

    // Convert 12 hour clock to 24 hour clock if necessary

    if (!(registerB & 0x02) && (curr.hour & 0x80)) {
        curr.hour = ((curr.hour & 0x7F) + 12) % 24;
    }

    // Calculate the full (4-digit) year

    if (century_register != 0) {
        curr.year += century * 100;
    } else {
        curr.year += (CURRENT_YEAR / 100) * 100;
        if (curr.year < CURRENT_YEAR) curr.year += 100;
    }

    release(&curr.lk);
}


date_time current_datetime() {
    read_rtc();
    acquire(&curr.lk);
    date_time dt = {
        .tm_sec = curr.second,
        .tm_min = curr.minute,
        .tm_hour = curr.hour,
        .tm_mday = curr.day,
        .tm_mon = curr.month - 1,
        .tm_year = curr.year - 1900
    };
    release(&curr.lk);
    return dt;
}


// Sourced from Newlib's mktime.c
// Convert datetime struct to UNIX epoch timestamp
time_t datetime2epoch(date_time* tim_p) {
    time_t tim = 0;
    long days = 0;
    int year = 0;

    /* compute hours, minutes, seconds */
    tim += tim_p->tm_sec + (tim_p->tm_min * _SEC_IN_MINUTE) +
        (tim_p->tm_hour * _SEC_IN_HOUR);

    /* compute days in year */
    days += tim_p->tm_mday - 1;
    days += _DAYS_BEFORE_MONTH[tim_p->tm_mon];
    if (tim_p->tm_mon > 1 && _DAYS_IN_YEAR(tim_p->tm_year) == 366)
        days++;

    /* compute day of the year */
    tim_p->tm_yday = days;

    if (tim_p->tm_year > 10000 || tim_p->tm_year < -10000)
        return (time_t)-1;

    /* compute days in other years */
    if ((year = tim_p->tm_year) > 70)     {
        for (year = 70; year < tim_p->tm_year; year++)
            days += _DAYS_IN_YEAR(year);
    }   else if (year < 70)     {
        for (year = 69; year > tim_p->tm_year; year--)
            days -= _DAYS_IN_YEAR(year);
        days -= _DAYS_IN_YEAR(year);
    }

    /* compute total seconds */
    tim += (time_t)days * _SEC_IN_DAY;

    /* compute day of the week */
    if ((tim_p->tm_wday = (days + 4) % 7) < 0)
        tim_p->tm_wday += 7;

    return tim;
}
