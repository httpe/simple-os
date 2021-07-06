#include <string.h>
#include <common.h>
#include <kernel/timer.h>
#include <kernel/time.h>
#include <kernel/lock.h>
#include <arch/i386/kernel/port_io.h>

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
    PORT_CMOS_DATA    = 0x71
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
    if(century_register != 0) {
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
        if(century_register != 0) {
            century = get_RTC_register(century_register);
        }
    } while( (last_second != curr.second) || (last_minute != curr.minute) || (last_hour != curr.hour) ||
           (last_day != curr.day) || (last_month != curr.month) || (last_year != curr.year) ||
           (last_century != century) );

    registerB = get_RTC_register(0x0B);

    // Convert BCD to binary values if necessary

    if (!(registerB & 0x04)) {
        curr.second = (curr.second & 0x0F) + ((curr.second / 16) * 10);
        curr.minute = (curr.minute & 0x0F) + ((curr.minute / 16) * 10);
        curr.hour = ( (curr.hour & 0x0F) + (((curr.hour & 0x70) / 16) * 10) ) | (curr.hour & 0x80);
        curr.day = (curr.day & 0x0F) + ((curr.day / 16) * 10);
        curr.month = (curr.month & 0x0F) + ((curr.month / 16) * 10);
        curr.year = (curr.year & 0x0F) + ((curr.year / 16) * 10);
        if(century_register != 0) {
            century = (century & 0x0F) + ((century / 16) * 10);
        }
    }

    // Convert 12 hour clock to 24 hour clock if necessary

    if (!(registerB & 0x02) && (curr.hour & 0x80)) {
        curr.hour = ((curr.hour & 0x7F) + 12) % 24;
    }

    // Calculate the full (4-digit) year

    if(century_register != 0) {
        curr.year += century * 100;
    } else {
        curr.year += (CURRENT_YEAR / 100) * 100;
        if(curr.year < CURRENT_YEAR) curr.year += 100;
    }

    release(&curr.lk);
}


date_time current_datetime()
{
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

