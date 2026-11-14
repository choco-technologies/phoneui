#include "dmod.h"
#include "dmdrvi_ioctl.h"
#include "phoneui_backend.h"
#include <string.h>
#include <errno.h>

/*
 * The stub backend: made-up data in the shape of the real one. Nothing is done: the quick settings tiles and the backlight keep what
 * they were set to, the store "installs" nothing. Except the Wi-Fi, which is real - its tile and the icon of the status bar are the node of
 * the Wi-Fi driver (wifi_* below) - and the list of the applications (phoneui_backend_apps). See phoneui_backend.h.
 */

/* ---- The Wi-Fi: the node of the driver (the interface wlan0: /dev/wlan0, where the `wifi` tool looks for it) ---- */

#define WIFI_NODE       "/dev/wlan0"
#define WIFI_TILE       0                       /* the first tile of the quick settings */

/* The state of the radio; a negative errno when there is no such node (no Wi-Fi driver) or it does not answer */
static int wifi_status(dmdrvi_wifi_status_t* status)
{
    void* node = Dmod_FileOpen(WIFI_NODE, "r+");
    if (node == NULL)
        return -ENODEV;
    memset(status, 0, sizeof(*status));
    int ret = Dmod_Ioctl(node, DMDRVI_IOCTL_WIFI_GET_STATUS, status);
    Dmod_FileClose(node);
    return ret;
}

static int wifi_set_enabled(bool enabled)
{
    void* node = Dmod_FileOpen(WIFI_NODE, "r+");
    if (node == NULL)
        return -ENODEV;
    int value = enabled ? 1 : 0;
    int ret = Dmod_Ioctl(node, DMDRVI_IOCTL_WIFI_SET_ENABLED, &value);
    Dmod_FileClose(node);
    return ret;
}

struct phoneui_backend
{
    int32_t  on[PHONEUI_QUICK];
    int32_t  level;
    int32_t  installed[PHONEUI_STORE_ROWS * 3];     /* by tab * rows + row */
};

/* The tables hold their texts in the arrays themselves: a table of pointers to texts would need the module's relocations */
typedef struct
{
    int32_t kind;
    char    initial[4];
    char    title[16];
    char    time[12];
    int32_t badge;
    char    sub[20];                                /* the conversation's second line of the header */
    char    last[40];                               /* the preview in the inbox */
} contact_t;

static const contact_t CONTACTS[] = {
    { phoneui_kind_sms,  "A", "Anna K.",  "09:12",     0, "SMS · 4G",       "Będę o 18, zostaw klucze u sąsiada" },
    { phoneui_kind_mesh, "#", "#general", "08:55",     3, "Mesh · 3 nodes", "Marek: kto na szlaku koło schroniska?" },
    { phoneui_kind_call, "T", "Tomek",    "08:30",     0, "Missed call",    "Missed call" },
    { phoneui_kind_sms,  "M", "Mama",     "yesterday", 1, "SMS · 4G",       "Zadzwoń jak będziesz mógł" },
    { phoneui_kind_chat, "P", "Piotr",    "yesterday", 0, "Chat · Wi-Fi",   "trasa.gpx (24 kB)" },
};
#define CONTACTS_COUNT  (sizeof(CONTACTS) / sizeof(CONTACTS[0]))

static void copy(char* to, size_t size, const char* from)
{
    Dmod_SnPrintf(to, size, "%s", from);
}

phoneui_backend_t phoneui_backend_new(void)
{
    phoneui_backend_t backend = Dmod_Malloc(sizeof(*backend));
    if (backend == NULL)
        return NULL;
    memset(backend, 0, sizeof(*backend));
    static const int32_t on[PHONEUI_QUICK] = { 1, 1, 0, 1, 1, 0, 1, 1, 0 };
    memcpy(backend->on, on, sizeof(on));
    backend->level = 6;
    backend->installed[1] = 1;                      /* "Paper Reader" is OPEN */
    return backend;
}

void phoneui_backend_free(phoneui_backend_t backend)
{
    Dmod_Free(backend);
}

void phoneui_backend_status(phoneui_backend_t backend, phoneui_status_t* status)
{
    /* The clock runs from 09:41 at the start: minutes of the uptime */
    uint32_t minutes = (uint32_t)(Dmod_GetUptime() / 60000U) + 9U * 60U + 41U;
    memset(status, 0, sizeof(*status));
    Dmod_SnPrintf(status->clock, sizeof(status->clock), "%02u:%02u", (unsigned)((minutes / 60U) % 24U), (unsigned)(minutes % 60U));
    copy(status->date, sizeof(status->date), "Saturday, 7 November");
    copy(status->line, sizeof(status->line), "3 mesh nodes  ·  GPS fix");
    status->battery = 82;
    status->signal = 4;
    dmdrvi_wifi_status_t wifi;
    status->wifi = (wifi_status(&wifi) == 0 && wifi.state == DMDRVI_WIFI_STATE_CONNECTED) ? 1 : 0;
    status->bt = backend->on[2];
    status->gps = backend->on[4];
    status->lora = backend->on[3];
}

static void contact_row(const contact_t* c, phoneui_row_t* row)
{
    memset(row, 0, sizeof(*row));
    row->kind = c->kind;
    copy(row->initial, sizeof(row->initial), c->initial);
    copy(row->title, sizeof(row->title), c->title);
    copy(row->text, sizeof(row->text), c->last);
    copy(row->time, sizeof(row->time), c->time);
    row->badge = c->badge;
}

void phoneui_backend_notifications(phoneui_backend_t backend, phoneui_row_t rows[PHONEUI_NOTIFICATIONS])
{
    (void)backend;
    memset(rows, 0, PHONEUI_NOTIFICATIONS * sizeof(rows[0]));
    for (unsigned i = 0; i < PHONEUI_NOTIFICATIONS; i++)
        contact_row(&CONTACTS[i], &rows[i]);
    copy(rows[0].title, sizeof(rows[0].title), "Anna");
    copy(rows[0].text, sizeof(rows[0].text), "Będę o 18, zostaw klucze…");
    copy(rows[1].text, sizeof(rows[1].text), "Marek: kto na szlaku?");
    copy(rows[2].title, sizeof(rows[2].title), "Missed call");
    copy(rows[2].text, sizeof(rows[2].text), "Tomek");
}

static bool in_tab(const contact_t* c, int tab)
{
    return tab == 0 || (tab == 1 && c->kind == phoneui_kind_sms) || (tab == 2 && c->kind == phoneui_kind_mesh) ||
           (tab == 3 && c->kind == phoneui_kind_chat);
}

void phoneui_backend_inbox(phoneui_backend_t backend, int tab, phoneui_row_t rows[PHONEUI_THREADS])
{
    (void)backend;
    memset(rows, 0, PHONEUI_THREADS * sizeof(rows[0]));
    unsigned n = 0;
    for (unsigned i = 0; i < CONTACTS_COUNT && n < PHONEUI_THREADS; i++)
    {
        if (in_tab(&CONTACTS[i], tab))
            contact_row(&CONTACTS[i], &rows[n++]);
    }
}

static const contact_t* contact_at(int tab, int row)
{
    int n = 0;
    for (unsigned i = 0; i < CONTACTS_COUNT; i++)
    {
        if (in_tab(&CONTACTS[i], tab) && n++ == row)
            return &CONTACTS[i];
    }
    return NULL;
}

static void bubble(phoneui_bubble_t* b, int side, const char* l1, const char* l2)
{
    b->side = side;
    copy(b->line1, sizeof(b->line1), l1);
    copy(b->line2, sizeof(b->line2), l2);
}

void phoneui_backend_thread(phoneui_backend_t backend, int tab, int row, phoneui_thread_t* thread)
{
    (void)backend;
    const contact_t* c = contact_at(tab, row);
    memset(thread, 0, sizeof(*thread));
    if (c == NULL)
        return;
    copy(thread->title, sizeof(thread->title), c->title);
    copy(thread->sub, sizeof(thread->sub), c->sub);
    copy(thread->initial, sizeof(thread->initial), c->initial);
    copy(thread->stamp, sizeof(thread->stamp), "Today 09:05");
    copy(thread->state, sizeof(thread->state), "✓ 09:12");
    bubble(&thread->bubbles[0], 1, "Hej, jesteś już w drodze?", "");
    bubble(&thread->bubbles[1], 2, "Tak, będę na miejscu", "za 20 min");
    bubble(&thread->bubbles[2], 1, "Super. Zostawić klucze", "u sąsiada?");
    bubble(&thread->bubbles[3], 2, "Tak, proszę. Dzięki!", "");
}

void phoneui_backend_quick(phoneui_backend_t backend, phoneui_quick_t* quick)
{
    memcpy(quick->on, backend->on, sizeof(quick->on));
    dmdrvi_wifi_status_t wifi;
    quick->on[WIFI_TILE] = (wifi_status(&wifi) == 0 && wifi.state != DMDRVI_WIFI_STATE_OFF) ? 1 : 0;
    quick->level = backend->level;
    copy(quick->battery, sizeof(quick->battery), "Battery 82%  ·  about 2 days left");
}

void phoneui_backend_quick_toggle(phoneui_backend_t backend, int tile, char* message, size_t size)
{
    if (tile < 0 || tile >= (int)PHONEUI_QUICK)
        return;
    if (tile == WIFI_TILE)
    {
        dmdrvi_wifi_status_t wifi;
        int ret = wifi_status(&wifi);
        if (ret == 0)
            ret = wifi_set_enabled(wifi.state == DMDRVI_WIFI_STATE_OFF);        /* off: on, anything else: off */
        if (ret == -ENODEV)
            copy(message, size, "Wi-Fi: not available");
        else if (ret == -EBUSY)
            copy(message, size, "Wi-Fi: starting, try again");
        else if (ret != 0)
            Dmod_SnPrintf(message, size, "Wi-Fi: cannot switch (%d)", ret);
        return;
    }
    backend->on[tile] = !backend->on[tile];
}

void phoneui_backend_quick_level(phoneui_backend_t backend, int level)
{
    if (level >= 0 && level <= (int)PHONEUI_LEVELS)
        backend->level = level;
}

void phoneui_backend_store(phoneui_backend_t backend, int tab, phoneui_store_t* store)
{
    static const struct { char title[16]; char sub[32]; } ITEMS[3][3] = {
        { { "Mailbox", "IMAP, offline queue · 84 kB" }, { "Paper Reader", "EPUB, TXT · 212 kB" }, { "Journal", "Markdown notes · 66 kB" } },
        { { "Terminal+", "Tabs, scrollback · 48 kB" }, { "Chess", "Offline, engine · 120 kB" }, { "Weather", "Forecast over 4G · 38 kB" } },
        { { "Journal", "Markdown notes · 66 kB" }, { "", "" }, { "", "" } },
    };
    memset(store, 0, sizeof(*store));
    tab = tab < 0 ? 0 : tab > 2 ? 2 : tab;
    copy(store->kicker, sizeof(store->kicker), "FEATURED");
    copy(store->title, sizeof(store->title), "LoRa Mesh Maps");
    copy(store->text, sizeof(store->text), "Offline maps shared over the mesh");
    for (unsigned i = 0; i < PHONEUI_STORE_ROWS; i++)
    {
        if (ITEMS[tab][i].title[0] == '\0')
            continue;
        store->rows[i].kind = 1;
        copy(store->rows[i].title, sizeof(store->rows[i].title), ITEMS[tab][i].title);
        copy(store->rows[i].sub, sizeof(store->rows[i].sub), ITEMS[tab][i].sub);
        copy(store->rows[i].button, sizeof(store->rows[i].button),
             tab == 2 ? "UPDATE" : backend->installed[tab * PHONEUI_STORE_ROWS + i] ? "OPEN" : "GET");
    }
}

void phoneui_backend_store_action(phoneui_backend_t backend, int tab, int row, char* message, size_t size)
{
    if (tab < 0 || tab > 2 || row < 0 || row >= (int)PHONEUI_STORE_ROWS)
        return;
    int32_t* installed = &backend->installed[tab * PHONEUI_STORE_ROWS + row];
    copy(message, size, *installed ? "Opening: not available yet" : "Installed (a stub: nothing was)");
    *installed = 1;
}

/* ---- The applications: the icons ---- */

#define ICONS_DIR       "/eviews/icons"             /* dmod-boot: the icons of the flash modules (dmf-get --icons-dir) */
#define ICON_SUFFIX     ".dmvi"

size_t phoneui_backend_apps(phoneui_backend_t backend, phoneui_app_t apps[PHONEUI_APPS])
{
    (void)backend;
    const char* dir = Dmod_GetEnv("PHONEUI_ICONS");
    if (dir == NULL || dir[0] == '\0')
        dir = ICONS_DIR;
    size_t dir_length = strlen(dir);
    while (dir_length > 1 && dir[dir_length - 1U] == '/')
        dir_length--;

    /* The directory without the slashes at its end (Dmod's printf has no "%.*s") */
    char base[PHONEUI_ICON_PATH];
    if (dir_length >= sizeof(base))
        return 0;
    memcpy(base, dir, dir_length);
    base[dir_length] = '\0';

    void* handle = Dmod_OpenDir(dir);
    if (handle == NULL)
        return 0;
    const size_t suffix = sizeof(ICON_SUFFIX) - 1U;
    size_t count = 0;
    const char* entry;
    while ((entry = Dmod_ReadDir(handle)) != NULL)
    {
        size_t length = strlen(entry);
        /* An icon is <module>.dmvi: a directory (the variants are in subdirectories) and anything else is not */
        if (length <= suffix || strcmp(entry + length - suffix, ICON_SUFFIX) != 0 || length - suffix >= PHONEUI_TEXT ||
            dir_length + 1U + length - suffix >= PHONEUI_ICON_PATH)
            continue;
        char name[PHONEUI_TEXT];
        memcpy(name, entry, length - suffix);
        name[length - suffix] = '\0';

        /* By name: inserted where it belongs */
        size_t at = 0;
        while (at < count && strcmp(apps[at].name, name) < 0)
            at++;
        if (count == PHONEUI_APPS && at == count)
            continue;                                       /* (past the last of the 24 that fit) */
        size_t last = (count == PHONEUI_APPS) ? count - 1U : count;
        for (size_t i = last; i > at; i--)
            apps[i] = apps[i - 1U];
        memset(&apps[at], 0, sizeof(apps[at]));
        copy(apps[at].name, sizeof(apps[at].name), name);
        Dmod_SnPrintf(apps[at].icon, sizeof(apps[at].icon), "%s/%s", base, name);
        if (count < PHONEUI_APPS)
            count++;
    }
    Dmod_CloseDir(handle);
    return count;
}

void phoneui_backend_request(phoneui_backend_t backend, const char* what, char* message, size_t size)
{
    (void)backend;
    if (strcmp(what, "search") == 0)
        copy(message, size, "Search: the keyboard comes next");
    else if (strcmp(what, "compose") == 0)
        copy(message, size, "New message: not available yet");
    else if (strcmp(what, "call") == 0)
        copy(message, size, "Call: not available yet");
    else if (strcmp(what, "send") == 0)
        copy(message, size, "Send: the keyboard comes next");
    else
        Dmod_SnPrintf(message, size, "%s: not available yet", what);
}
