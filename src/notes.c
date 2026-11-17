#include "dmod.h"
#include "dmosi.h"
#include "notes.h"
#include <string.h>

typedef struct
{
    bool     used;
    bool     stamped;                       /* its time was set (it was shown once) */
    uint32_t order;                         /* the bigger the newer */
    uint32_t channel;
    char     source[PHONEUI_NOTE_SOURCE];
    char     title[PHONEUI_TEXT];
    char     text[PHONEUI_TEXT];
    char     time[12];
} note_t;

static note_t g_notes[PHONEUI_NOTES];
static dmosi_mutex_t g_lock;
static uint32_t g_order;
static uint32_t g_serial;

/* Copy what fits of a text without cutting a character of UTF-8 in half (no formatting: this runs on the stack of whoever notifies) */
static void copy_text(char* to, size_t size, const char* from)
{
    size_t n = 0;
    if (from != NULL)
    {
        while (from[n] != '\0' && n + 1u < size)
            n++;
        if (from[n] != '\0')
            while (n > 0u && ((unsigned char)from[n] & 0xC0u) == 0x80u)
                n--;
        memcpy(to, from, n);
    }
    to[n] = '\0';
}

bool phoneui_notes_init(void)
{
    if (g_lock == NULL)
        g_lock = dmosi_mutex_create(false);
    return g_lock != NULL;
}

void phoneui_notes_free(void)
{
    if (g_lock == NULL)
        return;
    dmosi_mutex_destroy(g_lock);
    g_lock = NULL;
    memset(g_notes, 0, sizeof(g_notes));
}

static note_t* find(const char* source, uint32_t channel)
{
    for (unsigned i = 0; i < PHONEUI_NOTES; i++)
        if (g_notes[i].used && g_notes[i].channel == channel && strcmp(g_notes[i].source, source) == 0)
            return &g_notes[i];
    return NULL;
}

/* A free place, or the one of the oldest notification */
static note_t* place(void)
{
    note_t* oldest = &g_notes[0];
    for (unsigned i = 0; i < PHONEUI_NOTES; i++)
    {
        if (!g_notes[i].used)
            return &g_notes[i];
        if (g_notes[i].order < oldest->order)
            oldest = &g_notes[i];
    }
    return oldest;
}

void phoneui_notes_put(const char* source, uint32_t channel, const char* brief, const char* message)
{
    if (g_lock == NULL || source == NULL || brief == NULL)
        return;
    char name[PHONEUI_NOTE_SOURCE];
    copy_text(name, sizeof(name), source);          /* (what is kept is what is compared) */
    dmosi_mutex_lock(g_lock);
    note_t* note = find(name, channel);
    if (note == NULL)
        note = place();
    note->used = true;
    note->stamped = false;
    note->order = ++g_order;
    note->channel = channel;
    memcpy(note->source, name, sizeof(name));
    copy_text(note->title, sizeof(note->title), brief);
    copy_text(note->text, sizeof(note->text), message);
    g_serial++;
    dmosi_mutex_unlock(g_lock);
}

void phoneui_notes_remove(const char* source, uint32_t channel)
{
    if (g_lock == NULL || source == NULL)
        return;
    char name[PHONEUI_NOTE_SOURCE];
    copy_text(name, sizeof(name), source);
    dmosi_mutex_lock(g_lock);
    note_t* note = find(name, channel);
    if (note != NULL)
    {
        note->used = false;
        g_serial++;
    }
    dmosi_mutex_unlock(g_lock);
}

void phoneui_notes_clear(void)
{
    if (g_lock == NULL)
        return;
    dmosi_mutex_lock(g_lock);
    memset(g_notes, 0, sizeof(g_notes));
    g_serial++;
    dmosi_mutex_unlock(g_lock);
}

uint32_t phoneui_notes_serial(void)
{
    return g_serial;
}

size_t phoneui_notes_count(void)
{
    size_t count = 0;
    if (g_lock == NULL)
        return 0;
    dmosi_mutex_lock(g_lock);
    for (unsigned i = 0; i < PHONEUI_NOTES; i++)
        count += g_notes[i].used ? 1u : 0u;
    dmosi_mutex_unlock(g_lock);
    return count;
}

/* The newest notification that is older than `before` (the order of the previous row; 0: the newest of all) */
static note_t* next_newest(uint32_t before)
{
    note_t* best = NULL;
    for (unsigned i = 0; i < PHONEUI_NOTES; i++)
    {
        note_t* note = &g_notes[i];
        if (note->used && (before == 0u || note->order < before) && (best == NULL || note->order > best->order))
            best = note;
    }
    return best;
}

int32_t phoneui_notes_kind(const char* source)
{
    if (strcmp(source, "meshcore") == 0 || strcmp(source, "libmeshcore") == 0 || strcmp(source, "mesh") == 0)
        return phoneui_kind_mesh;
    if (strcmp(source, "phone") == 0 || strcmp(source, "call") == 0)
        return phoneui_kind_call;
    if (strcmp(source, "sms") == 0)
        return phoneui_kind_sms;
    return phoneui_kind_chat;
}

void phoneui_notes_rows(phoneui_row_t rows[PHONEUI_NOTIFICATIONS], const char* clock)
{
    memset(rows, 0, PHONEUI_NOTIFICATIONS * sizeof(rows[0]));
    if (g_lock == NULL)
        return;
    dmosi_mutex_lock(g_lock);
    for (unsigned i = 0; i < PHONEUI_NOTES; i++)
    {
        if (g_notes[i].used && !g_notes[i].stamped)
        {
            copy_text(g_notes[i].time, sizeof(g_notes[i].time), clock);
            g_notes[i].stamped = true;
        }
    }
    uint32_t before = 0;
    for (unsigned row = 0; row < PHONEUI_NOTIFICATIONS; row++)
    {
        const note_t* note = next_newest(before);
        if (note == NULL)
            break;
        before = note->order;
        rows[row].kind = phoneui_notes_kind(note->source);
        memcpy(rows[row].title, note->title, sizeof(rows[row].title));
        memcpy(rows[row].text, note->text, sizeof(rows[row].text));
        memcpy(rows[row].time, note->time, sizeof(rows[row].time));
    }
    dmosi_mutex_unlock(g_lock);
}

bool phoneui_notes_source(unsigned row, char* source, size_t size, uint32_t* channel)
{
    bool found = false;
    if (g_lock == NULL || size == 0u)
        return false;
    dmosi_mutex_lock(g_lock);
    uint32_t before = 0;
    const note_t* note = NULL;
    for (unsigned i = 0; i <= row; i++)
    {
        note = next_newest(before);
        if (note == NULL)
            break;
        before = note->order;
    }
    if (note != NULL)
    {
        copy_text(source, size, note->source);
        if (channel != NULL)
            *channel = note->channel;
        found = true;
    }
    dmosi_mutex_unlock(g_lock);
    return found;
}
