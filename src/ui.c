#include "dmod.h"
#include "ui.h"
#include <string.h>
#include <errno.h>

#define CACHE_ENTRIES       160u            /* Properties the view has (about 100) */
#define NAME_MAX_LEN        24u
#define VALUE_MAX_LEN       60u

/* What was set in the view last: only a change is sent (every property change is a redraw, on an e-paper a refresh) */
typedef struct
{
    char    name[NAME_MAX_LEN];
    char    text[VALUE_MAX_LEN];
    int32_t number;
    bool    is_text;
} cache_entry_t;

typedef struct
{
    uint32_t        claim;
    cache_entry_t   entries[CACHE_ENTRIES];
    uint32_t        count;
    uint32_t        failed;         /* sets the view did not take: it has no such property (the contract is broken) */
} cache_t;

struct ui
{
    cache_t             cache;
    phoneui_backend_t   backend;
    int                 inbox_tab;
    int                 store_tab;
    int                 thread_tab;         /* the tab and the row of the open conversation */
    int                 thread_row;
    unsigned            toasts;
};

static cache_entry_t* cache_find(cache_t* cache, const char* name)
{
    for (uint32_t i = 0; i < cache->count; i++)
    {
        if (strcmp(cache->entries[i].name, name) == 0)
            return &cache->entries[i];
    }
    if (cache->count >= CACHE_ENTRIES)
        return NULL;
    cache_entry_t* e = &cache->entries[cache->count++];
    memset(e, 0, sizeof(*e));
    Dmod_SnPrintf(e->name, sizeof(e->name), "%s", name);
    e->number = -0x7FFFFFFF;
    return e;
}

static void set_int(cache_t* cache, const char* name, int32_t value)
{
    cache_entry_t* e = cache_find(cache, name);
    if (e != NULL && !e->is_text && e->number == value)
        return;
    if (libdmview_prop_set_int(cache->claim, name, value) != 0)
    {
        cache->failed++;
        DMOD_LOG_WARN("phoneui: the view does not take %s\n", name);
    }
    else if (e != NULL)
    {
        e->is_text = false;
        e->number = value;
    }
}

static void set_text(cache_t* cache, const char* name, const char* value)
{
    cache_entry_t* e = cache_find(cache, name);
    if (e != NULL && e->is_text && strcmp(e->text, value) == 0)
        return;
    if (libdmview_prop_set_string(cache->claim, name, value) != 0)
    {
        cache->failed++;
        DMOD_LOG_WARN("phoneui: the view does not take %s\n", name);
    }
    else if (e != NULL)
    {
        e->is_text = true;
        Dmod_SnPrintf(e->text, sizeof(e->text), "%s", value);
    }
}

static void set_indexed_int(cache_t* cache, const char* prefix, unsigned i, const char* field, int32_t value)
{
    char name[NAME_MAX_LEN];
    Dmod_SnPrintf(name, sizeof(name), "%s%u.%s", prefix, i, field);
    set_int(cache, name, value);
}

static void set_indexed_text(cache_t* cache, const char* prefix, unsigned i, const char* field, const char* value)
{
    char name[NAME_MAX_LEN];
    Dmod_SnPrintf(name, sizeof(name), "%s%u.%s", prefix, i, field);
    set_text(cache, name, value);
}

/* The properties of the view, from the backend (docs/contract.md) */

void ui_show_status(ui_t ui)
{
    phoneui_status_t s;
    phoneui_backend_status(ui->backend, &s);
    set_text(&ui->cache, "clock", s.clock);
    set_text(&ui->cache, "date", s.date);
    set_text(&ui->cache, "home.line", s.line);
    set_int(&ui->cache, "battery", s.battery);
    set_int(&ui->cache, "charging", s.charging);
    set_int(&ui->cache, "signal", s.signal);
    set_int(&ui->cache, "wifi", s.wifi);
    set_int(&ui->cache, "bt", s.bt);
    set_int(&ui->cache, "gps", s.gps);
    set_int(&ui->cache, "lora", s.lora);
}

static void show_rows(ui_t ui, const char* prefix, const phoneui_row_t* rows, unsigned count, bool with_initial)
{
    for (unsigned i = 0; i < count; i++)
    {
        set_indexed_text(&ui->cache, prefix, i, "title", rows[i].title);
        set_indexed_text(&ui->cache, prefix, i, "text", rows[i].text);
        set_indexed_text(&ui->cache, prefix, i, "time", rows[i].time);
        if (with_initial)
            set_indexed_text(&ui->cache, prefix, i, "initial", rows[i].initial);
        set_indexed_int(&ui->cache, prefix, i, "badge", rows[i].badge);
        set_indexed_int(&ui->cache, prefix, i, "kind", rows[i].kind);          /* (last: the row is shown with its texts) */
    }
}

static void show_home(ui_t ui)
{
    phoneui_row_t rows[PHONEUI_NOTIFICATIONS];
    phoneui_backend_notifications(ui->backend, rows);
    show_rows(ui, "n", rows, PHONEUI_NOTIFICATIONS, false);
}

static void show_inbox(ui_t ui)
{
    phoneui_row_t rows[PHONEUI_THREADS];
    phoneui_backend_inbox(ui->backend, ui->inbox_tab, rows);
    show_rows(ui, "m", rows, PHONEUI_THREADS, true);
}

static void show_thread(ui_t ui)
{
    phoneui_thread_t t;
    phoneui_backend_thread(ui->backend, ui->thread_tab, ui->thread_row, &t);
    set_text(&ui->cache, "chat.title", t.title);
    set_text(&ui->cache, "chat.sub", t.sub);
    set_text(&ui->cache, "chat.ini", t.initial);
    set_text(&ui->cache, "chat.stamp", t.stamp);
    set_text(&ui->cache, "chat.state", t.state);
    for (unsigned i = 0; i < PHONEUI_BUBBLES; i++)
    {
        set_indexed_text(&ui->cache, "c", i, "l1", t.bubbles[i].line1);
        set_indexed_text(&ui->cache, "c", i, "l2", t.bubbles[i].line2);
        set_indexed_int(&ui->cache, "c", i, "side", t.bubbles[i].side);
    }
}

static void show_quick(ui_t ui)
{
    phoneui_quick_t q;
    phoneui_backend_quick(ui->backend, &q);
    for (unsigned i = 0; i < PHONEUI_QUICK; i++)
    {
        char name[NAME_MAX_LEN];
        Dmod_SnPrintf(name, sizeof(name), "q%u", i);
        set_int(&ui->cache, name, q.on[i]);
    }
    set_int(&ui->cache, "q.level", q.level);
    set_text(&ui->cache, "q.battery", q.battery);
}

static void show_store(ui_t ui)
{
    phoneui_store_t s;
    phoneui_backend_store(ui->backend, ui->store_tab, &s);
    set_text(&ui->cache, "store.kicker", s.kicker);
    set_text(&ui->cache, "store.title", s.title);
    set_text(&ui->cache, "store.text", s.text);
    for (unsigned i = 0; i < PHONEUI_STORE_ROWS; i++)
    {
        set_indexed_text(&ui->cache, "s", i, "title", s.rows[i].title);
        set_indexed_text(&ui->cache, "s", i, "sub", s.rows[i].sub);
        set_indexed_text(&ui->cache, "s", i, "button", s.rows[i].button);
        set_indexed_int(&ui->cache, "s", i, "kind", s.rows[i].kind);
    }
}

void ui_show_all(ui_t ui)
{
    ui_show_status(ui);
    show_home(ui);
    show_inbox(ui);
    show_thread(ui);
    show_quick(ui);
    show_store(ui);
}

/* A line at the bottom for the user (the view shows it for a moment): the text changes with every one, or the view would not see it */
static void toast(ui_t ui, const char* message)
{
    char text[VALUE_MAX_LEN];
    Dmod_SnPrintf(text, sizeof(text), "%s%s", message, (ui->toasts++ & 1U) ? " " : "");
    set_text(&ui->cache, "toast", text);
}

/* What the view asked for */
void ui_handle(ui_t ui, const libdmview_event_t* ev)
{
    char message[VALUE_MAX_LEN] = "";
    const char* name = ev->name;
    int value = (int)ev->value;

    if (strcmp(name, "page") == 0)
        ui_show_all(ui);                                           /* a page opens: what it shows is up to date */
    else if (strcmp(name, "launch") == 0)
        phoneui_backend_launch(ui->backend, value, message, sizeof(message));
    else if (strcmp(name, "inbox.tab") == 0)
    {
        ui->inbox_tab = value;
        show_inbox(ui);
    }
    else if (strcmp(name, "thread") == 0)
    {
        ui->thread_tab = ui->inbox_tab;
        ui->thread_row = value;
        show_thread(ui);
    }
    else if (strcmp(name, "quick") == 0)
    {
        phoneui_backend_quick_toggle(ui->backend, value);
        show_quick(ui);
        ui_show_status(ui);
    }
    else if (strcmp(name, "level") == 0)
    {
        phoneui_backend_quick_level(ui->backend, value);
        show_quick(ui);
    }
    else if (strcmp(name, "store.tab") == 0)
    {
        ui->store_tab = value;
        show_store(ui);
    }
    else if (strcmp(name, "store.action") == 0)
    {
        phoneui_backend_store_action(ui->backend, ui->store_tab, value, message, sizeof(message));
        show_store(ui);
    }
    else if (strcmp(name, "notification") == 0 || strcmp(name, "apps.filter") == 0)
        ;                                                       /* nothing to do yet */
    else
        phoneui_backend_request(ui->backend, name, message, sizeof(message));
    if (message[0] != '\0')
        toast(ui, message);
}

ui_t ui_new(uint32_t claim, phoneui_backend_t backend)
{
    ui_t ui = Dmod_Malloc(sizeof(*ui));
    if (ui == NULL)
        return NULL;
    memset(ui, 0, sizeof(*ui));
    ui->cache.claim = claim;
    ui->backend = backend;
    return ui;
}

uint32_t ui_failures(ui_t ui)
{
    return ui->cache.failed;
}

void ui_free(ui_t ui)
{
    Dmod_Free(ui);
}
