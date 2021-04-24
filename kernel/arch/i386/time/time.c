#include <string.h>
#include <common.h>
#include <kernel/timer.h>
#include <kernel/time.h>
#include <arch/i386/kernel/port_io.h>

// sourced from https://wiki.osdev.org/CMOS#Getting_Current_Date_and_Time_from_RTC

#define CURRENT_YEAR      2021                    // Change this each year!

int century_register = 0x00;                      // Set by ACPI table parsing code if possible

static uchar second;
static uchar minute;
static uchar hour;
static uchar day;
static uchar month;
static uint year;

enum {
    PORT_CMOS_ADDRESS = 0x70,
    PORT_CMOS_DATA    = 0x71
};

int get_update_in_progress_flag() {
    outb(PORT_CMOS_ADDRESS, 0x0A);
    return (inb(PORT_CMOS_DATA) & 0x80);
}

uchar get_RTC_register(int reg) {
    outb(PORT_CMOS_ADDRESS, reg);
    return inb(PORT_CMOS_DATA);
}

void read_rtc() {
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
    second = get_RTC_register(0x00);
    minute = get_RTC_register(0x02);
    hour = get_RTC_register(0x04);
    day = get_RTC_register(0x07);
    month = get_RTC_register(0x08);
    year = get_RTC_register(0x09);
    if(century_register != 0) {
        century = get_RTC_register(century_register);
    }

    do {
        last_second = second;
        last_minute = minute;
        last_hour = hour;
        last_day = day;
        last_month = month;
        last_year = year;
        last_century = century;

        while (get_update_in_progress_flag());         // Make sure an update isn't in progress
        second = get_RTC_register(0x00);
        minute = get_RTC_register(0x02);
        hour = get_RTC_register(0x04);
        day = get_RTC_register(0x07);
        month = get_RTC_register(0x08);
        year = get_RTC_register(0x09);
        if(century_register != 0) {
            century = get_RTC_register(century_register);
        }
    } while( (last_second != second) || (last_minute != minute) || (last_hour != hour) ||
           (last_day != day) || (last_month != month) || (last_year != year) ||
           (last_century != century) );

    registerB = get_RTC_register(0x0B);

    // Convert BCD to binary values if necessary

    if (!(registerB & 0x04)) {
        second = (second & 0x0F) + ((second / 16) * 10);
        minute = (minute & 0x0F) + ((minute / 16) * 10);
        hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
        day = (day & 0x0F) + ((day / 16) * 10);
        month = (month & 0x0F) + ((month / 16) * 10);
        year = (year & 0x0F) + ((year / 16) * 10);
        if(century_register != 0) {
            century = (century & 0x0F) + ((century / 16) * 10);
        }
    }

    // Convert 12 hour clock to 24 hour clock if necessary

    if (!(registerB & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    // Calculate the full (4-digit) year

    if(century_register != 0) {
        year += century * 100;
    } else {
        year += (CURRENT_YEAR / 100) * 100;
        if(year < CURRENT_YEAR) year += 100;
    }
}


date_time current_datetime()
{
    read_rtc();
    date_time dt = {
        .tm_sec = second,
        .tm_min = minute,
        .tm_hour = hour,
        .tm_mday = day,
        .tm_mon = month - 1,
        .tm_year = year - 1900
    };
    return dt;
}

