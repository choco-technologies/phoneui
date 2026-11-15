#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "phoneui_backend.h"
#include "battery_line.h"
#include "clock_line.h"
#include <string.h>

/* The stub backend: the data has the shape the view needs (texts fit, filters filter, state is kept) */

/* Every step makes a backend of its own: the state a step changes is not the next one's */
static phoneui_backend_t fresh(void)
{
    phoneui_backend_t backend = phoneui_backend_new();
    DMOD_TEST_EXPECT_NOT_NULL(backend);
    return backend;
}

DMOD_TEST_STEP(backend_is_created_and_freed)
{
    phoneui_backend_t backend = fresh();
    phoneui_backend_free(backend);
}

DMOD_TEST_STEP(status_is_complete)
{
    phoneui_backend_t backend = fresh();
    phoneui_status_t s;
    phoneui_backend_status(backend, &s);
    DMOD_TEST_EXPECT_EQ((int)strlen(s.clock), 5);
    DMOD_TEST_EXPECT_EQ(s.clock[2], ':');
    DMOD_TEST_EXPECT_TRUE(s.battery >= 0 && s.battery <= 100);
    DMOD_TEST_EXPECT_TRUE(s.signal >= 0 && s.signal <= 4);
    DMOD_TEST_EXPECT_TRUE(s.date[0] != '\0');
    phoneui_backend_free(backend);
}

DMOD_TEST_STEP(notifications_fill_the_home_screen)
{
    phoneui_backend_t backend = fresh();
    phoneui_row_t rows[PHONEUI_NOTIFICATIONS];
    phoneui_backend_notifications(backend, rows);
    for (unsigned i = 0; i < PHONEUI_NOTIFICATIONS; i++)
    {
        DMOD_TEST_EXPECT_TRUE(rows[i].kind != phoneui_kind_none);
        DMOD_TEST_EXPECT_TRUE(rows[i].title[0] != '\0');
    }
    phoneui_backend_free(backend);
}

static unsigned count_rows(const phoneui_row_t* rows)
{
    unsigned n = 0;
    for (unsigned i = 0; i < PHONEUI_THREADS; i++)
        n += rows[i].kind != phoneui_kind_none;
    return n;
}

DMOD_TEST_STEP(inbox_tabs_filter)
{
    phoneui_backend_t backend = fresh();
    phoneui_row_t rows[PHONEUI_THREADS];
    phoneui_backend_inbox(backend, 0, rows);
    DMOD_TEST_EXPECT_EQ((int)count_rows(rows), 5);
    phoneui_backend_inbox(backend, 1, rows);                    /* SMS */
    DMOD_TEST_EXPECT_EQ((int)count_rows(rows), 2);
    for (unsigned i = 0; i < 2; i++)
        DMOD_TEST_EXPECT_EQ(rows[i].kind, phoneui_kind_sms);
    phoneui_backend_inbox(backend, 2, rows);                    /* mesh */
    DMOD_TEST_EXPECT_EQ((int)count_rows(rows), 1);
    phoneui_backend_inbox(backend, 3, rows);                    /* chats */
    DMOD_TEST_EXPECT_EQ((int)count_rows(rows), 1);
    phoneui_backend_free(backend);
}

DMOD_TEST_STEP(thread_has_a_header_and_bubbles)
{
    phoneui_backend_t backend = fresh();
    phoneui_thread_t t;
    phoneui_backend_thread(backend, 0, 0, &t);
    DMOD_TEST_EXPECT_TRUE(t.title[0] != '\0');
    DMOD_TEST_EXPECT_TRUE(t.bubbles[0].side == 1 || t.bubbles[0].side == 2);
    phoneui_backend_thread(backend, 0, 99, &t);                 /* no such row: an empty conversation */
    DMOD_TEST_EXPECT_EQ((int)t.title[0], 0);
    phoneui_backend_free(backend);
}

DMOD_TEST_STEP(quick_settings_keep_state)
{
    phoneui_backend_t backend = fresh();
    phoneui_quick_t q;
    char message[64] = "";
    phoneui_backend_quick(backend, &q);
    int before = q.on[2];
    phoneui_backend_quick_toggle(backend, 2, message, sizeof(message));
    phoneui_backend_quick(backend, &q);
    DMOD_TEST_EXPECT_EQ(q.on[2], !before);
    DMOD_TEST_EXPECT_EQ(message[0], '\0');                      /* (what is done says nothing) */
    phoneui_backend_quick_toggle(backend, 99, message, sizeof(message));    /* out of range: ignored */
    DMOD_TEST_EXPECT_EQ(message[0], '\0');
    phoneui_backend_quick_level(backend, 3);
    phoneui_backend_quick(backend, &q);
    DMOD_TEST_EXPECT_EQ(q.level, 3);
    phoneui_backend_quick_level(backend, 11);                   /* out of range: ignored */
    phoneui_backend_quick(backend, &q);
    DMOD_TEST_EXPECT_EQ(q.level, 3);
    phoneui_backend_free(backend);
}

/* The Wi-Fi tile is the node of the driver: on the host there is none, so it is off, the icon is hidden and a tap says so */
DMOD_TEST_STEP(wifi_tile_without_a_node)
{
    phoneui_backend_t backend = fresh();
    phoneui_quick_t q;
    phoneui_status_t s;
    char message[64] = "";
    phoneui_backend_quick(backend, &q);
    DMOD_TEST_EXPECT_EQ(q.on[0], 0);
    phoneui_backend_status(backend, &s);
    DMOD_TEST_EXPECT_EQ(s.wifi, 0);
    phoneui_backend_quick_toggle(backend, 0, message, sizeof(message));
    DMOD_TEST_EXPECT_TRUE(strcmp(message, "Wi-Fi: not available") == 0);
    phoneui_backend_quick(backend, &q);
    DMOD_TEST_EXPECT_EQ(q.on[0], 0);                            /* (nothing was switched) */
    phoneui_backend_free(backend);
}

DMOD_TEST_STEP(store_actions_change_the_button)
{
    phoneui_backend_t backend = fresh();
    phoneui_store_t s;
    char message[64] = "";
    phoneui_backend_store(backend, 0, &s);
    DMOD_TEST_EXPECT_TRUE(strcmp(s.rows[0].button, "GET") == 0);
    phoneui_backend_store_action(backend, 0, 0, message, sizeof(message));
    DMOD_TEST_EXPECT_TRUE(message[0] != '\0');
    phoneui_backend_store(backend, 0, &s);
    DMOD_TEST_EXPECT_TRUE(strcmp(s.rows[0].button, "OPEN") == 0);
    phoneui_backend_free(backend);
}

/* The applications are the modules that have an icon: the .dmvi files of the icons directory, by name (tests/icons) */
#ifndef PHONEUI_TEST_ICONS
#define PHONEUI_TEST_ICONS "icons"
#endif

DMOD_TEST_STEP(applications_are_the_modules_with_an_icon)
{
    phoneui_backend_t backend = fresh();
    phoneui_app_t apps[PHONEUI_APPS];
    Dmod_SetEnv("PHONEUI_ICONS", PHONEUI_TEST_ICONS, 1);
    /* alpha, beta, gamma have an icon; readme.txt is no icon, small/ is a directory of icon variants, nothing.dmvi... is not a name */
    size_t n = phoneui_backend_apps(backend, apps);
    DMOD_TEST_EXPECT_EQ((int)n, 3);
    DMOD_TEST_EXPECT_TRUE(strcmp(apps[0].name, "alpha") == 0);
    DMOD_TEST_EXPECT_TRUE(strcmp(apps[1].name, "beta") == 0);
    DMOD_TEST_EXPECT_TRUE(strcmp(apps[2].name, "gamma") == 0);
    DMOD_TEST_EXPECT_TRUE(strcmp(apps[1].icon, PHONEUI_TEST_ICONS "/beta") == 0);      /* the view appends the end of the file */

    /* A trailing slash of the directory does not double */
    Dmod_SetEnv("PHONEUI_ICONS", PHONEUI_TEST_ICONS "/", 1);
    DMOD_TEST_EXPECT_EQ((int)phoneui_backend_apps(backend, apps), 3);
    DMOD_TEST_EXPECT_TRUE(strcmp(apps[0].icon, PHONEUI_TEST_ICONS "/alpha") == 0);

    /* No icons directory: no applications (nothing is made up) */
    Dmod_SetEnv("PHONEUI_ICONS", PHONEUI_TEST_ICONS "/nowhere", 1);
    DMOD_TEST_EXPECT_EQ((int)phoneui_backend_apps(backend, apps), 0);
    Dmod_SetEnv("PHONEUI_ICONS", "", 1);
    phoneui_backend_free(backend);
}

DMOD_TEST_STEP(requests_are_answered)
{
    phoneui_backend_t backend = fresh();
    char message[64] = "";
    phoneui_backend_request(backend, "search", message, sizeof(message));
    DMOD_TEST_EXPECT_TRUE(message[0] != '\0');
    phoneui_backend_free(backend);
}


/* ---- The battery: the node of a gauge is read through the interface of dmdrvi; the line under the tiles says it in words ---- */

static void line_of(uint8_t state, uint8_t percent, uint16_t to_empty, uint16_t to_full, uint32_t valid, char* out, size_t size)
{
    dmdrvi_battery_status_t b;
    memset(&b, 0, sizeof(b));
    b.valid = valid;
    b.state = state;
    b.percent = percent;
    b.minutes_to_empty = to_empty;
    b.minutes_to_full = to_full;
    phoneui_battery_line(&b, out, size);
}

#define ALL (DMDRVI_BATTERY_VALID_STATE | DMDRVI_BATTERY_VALID_PERCENT | DMDRVI_BATTERY_VALID_TIME)

DMOD_TEST_STEP(battery_line_discharging)
{
    char line[64];
    line_of(DMDRVI_BATTERY_STATE_DISCHARGING, 82, 250, 0, ALL, line, sizeof(line));
    DMOD_TEST_EXPECT_TRUE(strcmp(line, "Battery 82%  \xC2\xB7  4 h 10 min left") == 0);
    line_of(DMDRVI_BATTERY_STATE_DISCHARGING, 3, 35, 0, ALL, line, sizeof(line));
    DMOD_TEST_EXPECT_TRUE(strcmp(line, "Battery 3%  \xC2\xB7  35 min left") == 0);
    line_of(DMDRVI_BATTERY_STATE_DISCHARGING, 99, 3 * 24 * 60, 0, ALL, line, sizeof(line));
    DMOD_TEST_EXPECT_TRUE(strcmp(line, "Battery 99%  \xC2\xB7  about 3 days left") == 0);
    /* the chip does not know the time (or it is not valid): no time is made up */
    line_of(DMDRVI_BATTERY_STATE_DISCHARGING, 82, 0, 0, ALL, line, sizeof(line));
    DMOD_TEST_EXPECT_TRUE(strcmp(line, "Battery 82%") == 0);
    line_of(DMDRVI_BATTERY_STATE_DISCHARGING, 82, 250, 0, ALL & ~DMDRVI_BATTERY_VALID_TIME, line, sizeof(line));
    DMOD_TEST_EXPECT_TRUE(strcmp(line, "Battery 82%") == 0);
}

DMOD_TEST_STEP(battery_line_charging_full_idle)
{
    char line[64];
    line_of(DMDRVI_BATTERY_STATE_CHARGING, 40, 0, 80, ALL, line, sizeof(line));
    DMOD_TEST_EXPECT_TRUE(strcmp(line, "Battery 40%  \xC2\xB7  charging, full in 1 h 20 min") == 0);
    line_of(DMDRVI_BATTERY_STATE_CHARGING, 40, 0, 0, ALL, line, sizeof(line));
    DMOD_TEST_EXPECT_TRUE(strcmp(line, "Battery 40%  \xC2\xB7  charging") == 0);
    line_of(DMDRVI_BATTERY_STATE_FULL, 100, 0, 0, ALL, line, sizeof(line));
    DMOD_TEST_EXPECT_TRUE(strcmp(line, "Battery 100%  \xC2\xB7  full") == 0);
    line_of(DMDRVI_BATTERY_STATE_IDLE, 60, 0, 0, ALL, line, sizeof(line));
    DMOD_TEST_EXPECT_TRUE(strcmp(line, "Battery 60%  \xC2\xB7  not charging") == 0);
}

DMOD_TEST_STEP(battery_line_without_a_battery_or_a_number)
{
    char line[64];
    line_of(DMDRVI_BATTERY_STATE_NOT_PRESENT, 0, 0, 0, DMDRVI_BATTERY_VALID_STATE, line, sizeof(line));
    DMOD_TEST_EXPECT_TRUE(strcmp(line, "No battery") == 0);
    line_of(DMDRVI_BATTERY_STATE_UNKNOWN, 0, 0, 0, 0, line, sizeof(line));            /* nothing is valid: nothing is shown but the word */
    DMOD_TEST_EXPECT_TRUE(strcmp(line, "Battery") == 0);
    line_of(DMDRVI_BATTERY_STATE_UNKNOWN, 55, 0, 0, DMDRVI_BATTERY_VALID_PERCENT, line, sizeof(line));
    DMOD_TEST_EXPECT_TRUE(strcmp(line, "Battery 55%") == 0);
}

/* No node of a gauge (the host, a board without one): the stub's made-up battery stays */
DMOD_TEST_STEP(battery_without_a_node_is_the_stub)
{
    phoneui_backend_t backend = fresh();
    phoneui_status_t s;
    phoneui_quick_t q;
    phoneui_backend_status(backend, &s);
    DMOD_TEST_EXPECT_EQ(s.battery, 82);
    DMOD_TEST_EXPECT_EQ(s.charging, 0);
    phoneui_backend_quick(backend, &q);
    DMOD_TEST_EXPECT_TRUE(strncmp(q.battery, "Battery 82%", 11) == 0);
    phoneui_backend_free(backend);
}

/* The clock and the date from UTC: the offset, the summer time of the EU */
DMOD_TEST_STEP(clock_text_from_utc)
{
    char clock[16], date[40];
    /* 2026-11-15T13:05:00Z, a Sunday: CET is +1 h */
    phoneui_clock_text(1794747900, 60, true, clock, sizeof(clock), date, sizeof(date));
    DMOD_TEST_EXPECT_TRUE(strcmp(clock, "14:05") == 0);
    DMOD_TEST_EXPECT_TRUE(strcmp(date, "Sunday, 15 November") == 0);
    /* UTC, no offset */
    phoneui_clock_text(1794747900, 0, false, clock, sizeof(clock), date, sizeof(date));
    DMOD_TEST_EXPECT_TRUE(strcmp(clock, "13:05") == 0);
    /* 2026-07-01T12:00:00Z: the summer time makes it 14:00 in Poland; Wednesday */
    phoneui_clock_text(1782907200, 60, true, clock, sizeof(clock), date, sizeof(date));
    DMOD_TEST_EXPECT_TRUE(strcmp(clock, "14:00") == 0);
    DMOD_TEST_EXPECT_TRUE(strcmp(date, "Wednesday, 1 July") == 0);
    /* the day changes with the offset: 2026-12-31T23:30:00Z is 2027-01-01 00:30 in Poland (a Friday) */
    phoneui_clock_text(1798759800, 60, true, clock, sizeof(clock), date, sizeof(date));
    DMOD_TEST_EXPECT_TRUE(strcmp(clock, "00:30") == 0);
    DMOD_TEST_EXPECT_TRUE(strcmp(date, "Friday, 1 January") == 0);
    /* the switch: 2026-03-29T00:59:59Z is still winter (01:59), 01:00:00Z is summer (03:00) */
    phoneui_clock_text(1774745999, 60, true, clock, sizeof(clock), date, sizeof(date));
    DMOD_TEST_EXPECT_TRUE(strcmp(clock, "01:59") == 0);
    phoneui_clock_text(1774746000, 60, true, clock, sizeof(clock), date, sizeof(date));
    DMOD_TEST_EXPECT_TRUE(strcmp(clock, "03:00") == 0);
    /* 2026-10-25T00:59:59Z is still summer (02:59), 01:00:00Z is winter (02:00) */
    phoneui_clock_text(1792889999, 60, true, clock, sizeof(clock), date, sizeof(date));
    DMOD_TEST_EXPECT_TRUE(strcmp(clock, "02:59") == 0);
    phoneui_clock_text(1792890000, 60, true, clock, sizeof(clock), date, sizeof(date));
    DMOD_TEST_EXPECT_TRUE(strcmp(clock, "02:00") == 0);
}
