#ifndef CLOCK_LINE_H
#define CLOCK_LINE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief The clock and the date of the status bar from UTC: "14:05" and "Sunday, 15 November".
 *
 * @param utc_seconds   seconds since 1970-01-01T00:00:00Z
 * @param offset_min    the local time's offset from UTC in minutes (60: CET)
 * @param eu_dst        true: the summer time of the European Union is added (from the last Sunday of March, 01:00 UTC, to the last
 *                      Sunday of October, 01:00 UTC)
 */
void phoneui_clock_text(int64_t utc_seconds, int offset_min, bool eu_dst, char *clock, size_t clock_size, char *date, size_t date_size);

#endif
