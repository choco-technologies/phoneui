#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "phoneui_backend.h"
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

