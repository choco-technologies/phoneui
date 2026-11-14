#include "dmod.h"
#include "ui.h"
#include <string.h>
#include <errno.h>

#define CACHE_ENTRIES       256u            /* Properties the view has (about 180) */
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

#define TYPED_MAX       36U                         /* Bytes of the text of the search bar */

/* The pages of the view (docs/contract.md, `page`) */
#define PAGE_APPS       1
#define PAGE_APP        6                           /* an application, in a rent */

/* Where an application lives: the area of the page between the status bar and the buttons (the view's page 6 is empty there) */
#define APP_RECT_X      0
#define APP_RECT_Y      19
#define APP_RECT_W      240
#define APP_RECT_H      275
#define KEY_ESCAPE      0x29u                       /* USB HID usage: always the phone's, not the application's */

struct ui
{
    cache_t             cache;
    phoneui_backend_t   backend;
    int                 inbox_tab;
    int                 store_tab;
    int                 thread_tab;         /* the tab and the row of the open conversation */
    int                 thread_row;
    unsigned            toasts;
    char                typed[TYPED_MAX + 1];   /* What the keyboard wrote (UTF-8) */
    bool                typing;                 /* The search bar is where the keys go: it shows a cursor */
    phoneui_app_t       apps[PHONEUI_APPS];     /* The applications of the list, as the view shows them */
    unsigned            app_count;
    int                 page;                   /* The page of the view */
    uint32_t            rent;                   /* The rent of the application that runs, 0: none */
    uint32_t            goto_count;             /* Makes every request for a page another value of the property */
    /* The arguments of the application: Dmod_Spawn() keeps the pointers for as long as the process runs, so they are not on a stack */
    char                app_name[PHONEUI_TEXT];
    char                rent_name[48];
    char*               app_argv[3];
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

/* The applications that have an icon: a cell of the grid each - its name and its icon, which the view draws from the file */
static void show_apps(ui_t ui)
{
    ui->app_count = (unsigned)phoneui_backend_apps(ui->backend, ui->apps);
    for (unsigned i = 0; i < PHONEUI_APPS; i++)
    {
        if (i < ui->app_count)
        {
            set_indexed_text(&ui->cache, "a", i, "name", ui->apps[i].name);
            set_indexed_text(&ui->cache, "a", i, "icon", ui->apps[i].icon);
        }
        set_indexed_int(&ui->cache, "a", i, "on", i < ui->app_count);       /* (last: the cell is shown with its name and icon) */
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
    show_apps(ui);
}

/* A line at the bottom for the user (the view shows it for a moment): the text changes with every one, or the view would not see it */
static void toast(ui_t ui, const char* message)
{
    char text[VALUE_MAX_LEN];
    Dmod_SnPrintf(text, sizeof(text), "%s%s", message, (ui->toasts++ & 1U) ? " " : "");
    set_text(&ui->cache, "toast", text);
}

/* What the view asked for */
/* The characters the keyboard writes: appended to the search bar of the home page (UTF-8) */
static void show_typed(ui_t ui)
{
    char text[TYPED_MAX + 2];
    Dmod_SnPrintf(text, sizeof(text), "%s|", ui->typed);          /* the cursor says the keys go here */
    set_text(&ui->cache, "search.text", text);
    set_int(&ui->cache, "search.on", ui->typing);
}


static unsigned encode_utf8(uint32_t cp, char* out)
{
    if (cp < 0x80U)
    {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800U)
    {
        out[0] = (char)(0xC0U | (cp >> 6));
        out[1] = (char)(0x80U | (cp & 0x3FU));
        return 2;
    }
    if (cp < 0x10000U)
    {
        out[0] = (char)(0xE0U | (cp >> 12));
        out[1] = (char)(0x80U | ((cp >> 6) & 0x3FU));
        out[2] = (char)(0x80U | (cp & 0x3FU));
        return 3;
    }
    out[0] = (char)(0xF0U | (cp >> 18));
    out[1] = (char)(0x80U | ((cp >> 12) & 0x3FU));
    out[2] = (char)(0x80U | ((cp >> 6) & 0x3FU));
    out[3] = (char)(0x80U | (cp & 0x3FU));
    return 4;
}

static void type_char(ui_t ui, uint32_t codepoint)
{
    char bytes[4];
    size_t length = strlen(ui->typed);
    unsigned n = encode_utf8(codepoint, bytes);
    if (length + n > TYPED_MAX)
        return;                                                 /* the bar is full */
    memcpy(ui->typed + length, bytes, n);
    ui->typed[length + n] = '\0';
    show_typed(ui);
}

static void type_erase(ui_t ui)
{
    size_t length = strlen(ui->typed);
    while (length > 0 && ((uint8_t)ui->typed[length - 1U] & 0xC0U) == 0x80U)
        length--;                                               /* the bytes of a character go together */
    if (length > 0)
        length--;
    ui->typed[length] = '\0';
    show_typed(ui);
}

/* A key the keyboard gave that is no character: Backspace, Enter, Escape */
static void type_key(ui_t ui, int code, char* message, size_t size)
{
    if (code == 0x2A)
        type_erase(ui);
    else if (code == 0x29)
    {
        ui->typed[0] = '\0';                                    /* Escape: the field is cleared and deselected */
        ui->typing = false;
        show_typed(ui);
    }
    else if (code == 0x28 && ui->typed[0] != '\0')
    {
        Dmod_SnPrintf(message, size, "Search: %s", ui->typed);
        ui->typed[0] = '\0';
        show_typed(ui);
    }
}

/* ---- An application in a rent ---- */

/* The view goes to a page (the application is what changes the page when its application ends) */
static void goto_page(ui_t ui, int page)
{
    set_int(&ui->cache, "goto", (int32_t)(((++ui->goto_count & 0xFFFFu) << 4) | (uint32_t)page));
}

/* The rent is taken back: the application's claim is gone, which is what it ends by */
static void app_close(ui_t ui)
{
    if (ui->rent == 0)
        return;
    (void)libdmview_rent_set_focus(ui->rent, false);
    (void)libdmview_rent_release(ui->rent);
    ui->rent = 0;
}

/*
 * The application of a cell is started in a rent of the screen: the area of page 6 of the view. It is given the name of the rent
 * (`--display NAME`) and claims it; it is shown while the page is, has the keys (except Escape, which is the phone's) and ends when
 * the rent is taken back - on any other page, or when the user leaves.
 */
static void app_launch(ui_t ui, int index, char* message, size_t size)
{
    if (index < 0 || (unsigned)index >= ui->app_count)
    {
        Dmod_SnPrintf(message, size, "No such application");
        goto_page(ui, PAGE_APPS);
        return;
    }
    const phoneui_app_t* app = &ui->apps[index];
    app_close(ui);

    const libdmview_rect_t area = { APP_RECT_X, APP_RECT_Y, APP_RECT_W, APP_RECT_H };
    uint32_t rent = 0;
    char* name = ui->rent_name;
    int ret = libdmview_rent_create(ui->cache.claim, &area, &rent);
    if (ret == 0)
        ret = libdmview_rent_name(rent, name, sizeof(ui->rent_name));
    if (ret != 0)
    {
        Dmod_SnPrintf(message, size, "%s: no room on the screen (%d)", app->name, ret);
        goto_page(ui, PAGE_APPS);
        return;
    }
    Dmod_SnPrintf(ui->app_name, sizeof(ui->app_name), "%s", app->name);
    ui->app_argv[0] = ui->app_name;
    ui->app_argv[1] = "--display";
    ui->app_argv[2] = ui->rent_name;
    int pid = Dmod_SpawnModule(ui->app_name, 3, ui->app_argv, NULL);
    if (pid < 0)
    {
        (void)libdmview_rent_release(rent);
        Dmod_SnPrintf(message, size, "%s: cannot start (%d)", app->name, pid);
        goto_page(ui, PAGE_APPS);
        return;
    }
    (void)libdmview_rent_bind(rent, (uint32_t)pid);
    (void)libdmview_rent_set_escape(rent, KEY_ESCAPE);
    (void)libdmview_rent_set_focus(rent, true);
    (void)libdmview_rent_set_visible(rent, ui->page == PAGE_APP);
    ui->rent = rent;
    DMOD_LOG_INFO("phoneui: %s (pid %d) runs in the rent %s\n", app->name, pid, name);
}

bool ui_launch(ui_t ui, const char* name)
{
    show_apps(ui);
    for (unsigned i = 0; i < ui->app_count; i++)
    {
        if (strcmp(ui->apps[i].name, name) != 0)
            continue;
        char message[VALUE_MAX_LEN] = "";
        app_launch(ui, (int)i, message, sizeof(message));
        if (message[0] != '\0')
            toast(ui, message);
        else
            goto_page(ui, PAGE_APP);                                /* (what the touch's page event does) */
        return true;
    }
    return false;
}

void ui_handle(ui_t ui, const libdmview_event_t* ev)
{
    char message[VALUE_MAX_LEN] = "";
    const char* name = ev->name;
    int value = (int)ev->value;

    if (strcmp(name, "page") == 0)
    {
        ui->page = value;
        ui->typing = false;                                         /* the field is deselected with the page, and empty */
        ui->typed[0] = '\0';
        show_typed(ui);
        if (ui->rent != 0)
        {
            if (value == PAGE_APP)
                (void)libdmview_rent_set_visible(ui->rent, true);   /* its application's page: shown */
            else
                app_close(ui);                                      /* any other: the application ends */
        }
        ui_show_all(ui);                                           /* a page opens: what it shows is up to date */
    }
    else if (strcmp(name, "launch") == 0)
        app_launch(ui, value, message, sizeof(message));
    else if (strcmp(name, "rent.closed") == 0)
    {
        /* Its application ended by itself: the rent is of no use, the user is back at the applications */
        if (ui->rent != 0 && (uint32_t)value == ui->rent)
        {
            app_close(ui);
            if (ui->page == PAGE_APP)
                goto_page(ui, PAGE_APPS);
        }
    }
    else if (strcmp(name, "rent.open") == 0)
        ;                                                           /* (the view of the application is shown by the display's service) */
    else if (strcmp(name, "dock") == 0)
    {
        static const char* const DOCK[] = { "phone", "chat", "mesh", "maps" };
        phoneui_backend_request(ui->backend, (value >= 0 && value < 4) ? DOCK[value] : "dock", message, sizeof(message));
    }
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
        phoneui_backend_quick_toggle(ui->backend, value, message, sizeof(message));
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
    else if (strcmp(name, "notification") == 0 || strcmp(name, "apps.filter") == 0 || strcmp(name, "mods") == 0)
        ;                                                       /* nothing to do yet */
    else if (strcmp(name, "search") == 0)
    {
        ui->typing = true;                                          /* a tap on the search bar selects it: the keys go there */
        show_typed(ui);
    }
    else if (strcmp(name, "char") == 0)
    {
        if (ui->typing)                                             /* with no field selected the keys go nowhere */
            type_char(ui, (uint32_t)ev->value);
    }
    else if (strcmp(name, "key") == 0)
    {
        if (ui->typing)
            type_key(ui, value, message, sizeof(message));
        else if (value == (int)KEY_ESCAPE && ui->page == PAGE_APP)
        {
            app_close(ui);                                          /* Escape leaves the application */
            goto_page(ui, PAGE_APPS);
        }
    }
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
    if (ui != NULL)
        app_close(ui);
    Dmod_Free(ui);
}
