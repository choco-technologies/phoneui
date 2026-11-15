#include "clock_line.h"
#include "dmod.h"

/* (arrays of characters, not of pointers: a module has no relocation of the data) */
static const char WEEKDAYS[7][10] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char MONTHS[12][10] = { "January", "February", "March", "April", "May", "June", "July", "August", "September",
                                    "October", "November", "December" };

/* The civil date of a day number counted from 1970-01-01 (Howard Hinnant's days_from_civil, backwards) */
static void civil_from_days(int64_t z, int *year, unsigned *month, unsigned *day)
{
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = (int64_t)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    *day = doy - (153 * mp + 2) / 5 + 1;
    *month = mp < 10 ? mp + 3 : mp - 9;
    *year = (int)(y + (*month <= 2 ? 1 : 0));
}

static int64_t days_from_civil(int year, unsigned month, unsigned day)
{
    int64_t y = year - (month <= 2 ? 1 : 0);
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

/** 01:00 UTC of the last Sunday of a month (31 days long) of a year, in seconds since the epoch */
static int64_t last_sunday_01utc(int year, unsigned month)
{
    int64_t last = days_from_civil(year, month, 31);
    unsigned weekday = (unsigned)((last % 7 + 7 + 4) % 7);          /* 1970-01-01 was a Thursday (4); 0 = Sunday */
    return (last - weekday) * 86400 + 3600;
}

void phoneui_clock_text(int64_t utc_seconds, int offset_min, bool eu_dst, char *clock, size_t clock_size, char *date, size_t date_size)
{
    int64_t local = utc_seconds + (int64_t)offset_min * 60;
    if (eu_dst)
    {
        int year;
        unsigned month, day;
        civil_from_days(utc_seconds / 86400 - (utc_seconds % 86400 < 0 ? 1 : 0), &year, &month, &day);
        if (utc_seconds >= last_sunday_01utc(year, 3) && utc_seconds < last_sunday_01utc(year, 10))
            local += 3600;
    }
    int64_t days = local / 86400;
    int64_t rest = local % 86400;
    if (rest < 0)
    {
        rest += 86400;
        days--;
    }
    int year;
    unsigned month, day;
    civil_from_days(days, &year, &month, &day);
    unsigned weekday = (unsigned)((days % 7 + 7 + 4) % 7);
    if (clock != NULL && clock_size != 0)
        Dmod_SnPrintf(clock, clock_size, "%02u:%02u", (unsigned)(rest / 3600), (unsigned)(rest % 3600 / 60));
    if (date != NULL && date_size != 0)
        Dmod_SnPrintf(date, date_size, "%s, %u %s", WEEKDAYS[weekday], day, MONTHS[month - 1]);
}
