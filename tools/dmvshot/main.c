#include "dmod.h"
#include "dmosi.h"
#include "libdmview.h"
#include "dmdrvi_ioctl.h"
#include "../../src/ui.h"
#include <string.h>

/*
 * dmvshot - the interface on a PC: the real view, the real application logic (src/ui.c) and the stub backend, drawn by libdmview into
 * an off-screen buffer of the display's format (RGB565) and written as pictures. It plays the part of the display service (dmview)
 * and of the touch panel, so a page of the interface can be looked at and tested without the board.
 *
 *   dmvshot <view.dmv> <out directory> <step>...
 *
 * steps:   shot:<name>      write <name>.ppm (what libdmview drew) and <name>-mono.ppm (what the e-paper shows of it)
 *          click:<x>,<y>    a touch and its lifting
 *          emit:<name>:<value>  an event as if libdmview_key sent it (char, key, mods)
 *          swipe:<x0>,<y0>,<x1>,<y1>  a finger that moves from the first point to the second
 *          press:<x>,<y>    a touch that stays (a pressed button); release: lifts it
 *          wait:<ms>        let the time pass (timers, the toast)
 *          expect:<name>[:<value>]  the last event of the view was that one (else the exit code says so)
 *          threshold:<n>    from now on the e-paper's picture is white from the luminance n (default 128)
 */

#define SCREEN_W    240U
#define SCREEN_H    320U
#define STEP_MS     16U

typedef struct
{
    libdmview_t         view;
    uint16_t            pixels[SCREEN_W * SCREEN_H];
    libdmview_surface_t surface;
    uint32_t            now;
    uint32_t            claim;
    ui_t                ui;
    const char*         out;
    char                last[LIBDMVIEW_EVENT_NAME_MAX];    /* the last event of the view */
    int32_t             last_value;
    uint16_t            held_x, held_y; /* where press: touched */
    uint32_t            threshold;      /* a pixel is white when its luminance is at least this (the e-paper's) */
} shot_t;

/* 5-6-5 to 8 bits: the top bits repeated, so that white is 255 */
static void rgb(uint16_t p, uint8_t* r, uint8_t* g, uint8_t* b)
{
    *r = (uint8_t)(((p >> 11) & 31U) * 255U / 31U);
    *g = (uint8_t)(((p >> 5) & 63U) * 255U / 63U);
    *b = (uint8_t)((p & 31U) * 255U / 31U);
}

static int write_ppm(const shot_t* s, const char* name, bool mono)
{
    char path[200];
    Dmod_SnPrintf(path, sizeof(path), "%s/%s%s.ppm", s->out, name, mono ? "-mono" : "");
    void* f = Dmod_FileOpen(path, "w");
    if (f == NULL)
    {
        Dmod_Printf("dmvshot: cannot write %s\n", path);
        return -1;
    }
    char header[32];
    int len = Dmod_SnPrintf(header, sizeof(header), "P6\n%u %u\n255\n", SCREEN_W, SCREEN_H);
    (void)Dmod_FileWrite(header, 1, (size_t)len, f);
    uint8_t row[SCREEN_W * 3U];
    for (uint32_t y = 0; y < SCREEN_H; y++)
    {
        for (uint32_t x = 0; x < SCREEN_W; x++)
        {
            uint8_t r, g, b;
            rgb(s->pixels[y * SCREEN_W + x], &r, &g, &b);
            if (mono)
                r = g = b = ((r * 299U + g * 587U + b * 114U) / 1000U >= s->threshold) ? 255U : 0U;
            row[x * 3U] = r;
            row[x * 3U + 1U] = g;
            row[x * 3U + 2U] = b;
        }
        (void)Dmod_FileWrite(row, 1, sizeof(row), f);
    }
    Dmod_FileClose(f);
    return 0;
}

/* The events the view emitted since: the application runs them (and sets properties, which are taken in at the next update) */
/* A decimal number at the start of text; *end is where it ended (dmod modules have no strtol) */
static long number(const char* text, const char** end)
{
    long v = 0;
    while (*text >= '0' && *text <= '9')
        v = v * 10 + (*text++ - '0');
    if (end != NULL)
        *end = text;
    return v;
}

static void run_events(shot_t* s)
{
    libdmview_event_t ev;
    while (libdmview_wait_event(s->claim, &ev, 0) == 0)
    {
        Dmod_Printf("event %s %d\n", ev.name, (int)ev.value);
        Dmod_SnPrintf(s->last, sizeof(s->last), "%s", ev.name);
        s->last_value = ev.value;
        ui_handle(s->ui, &ev);
    }
}

static void frame(shot_t* s)
{
    libdmview_rect_t changed;
    (void)libdmview_update(s->view, s->now);
    (void)libdmview_render(s->view, &s->surface, &changed);
}

static void settle(shot_t* s, uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += STEP_MS)
    {
        s->now += STEP_MS;
        run_events(s);
        frame(s);
    }
}

static void touch(shot_t* s, uint16_t x, uint16_t y, uint8_t event)
{
    dmdrvi_input_state_t st;
    memset(&st, 0, sizeof(st));
    if (event != DMDRVI_INPUT_CONTACT_UP + 1)
    {
        st.contact_count = 1;
        st.contacts[0].x = x;
        st.contacts[0].y = y;
        st.contacts[0].event = event;
    }
    (void)libdmview_input(s->view, &st, s->now);
}

/* A finger down at x0,y0 that moves to x1,y1 in steps of a few pixels and is lifted */
static void swipe(shot_t* s, long x0, long y0, long x1, long y1)
{
    touch(s, (uint16_t)x0, (uint16_t)y0, DMDRVI_INPUT_CONTACT_DOWN);
    settle(s, 3 * STEP_MS);
    for (int k = 1; k <= 10; k++)
    {
        touch(s, (uint16_t)(x0 + (x1 - x0) * k / 10), (uint16_t)(y0 + (y1 - y0) * k / 10), DMDRVI_INPUT_CONTACT_MOVE);
        settle(s, 2 * STEP_MS);
    }
    touch(s, (uint16_t)x1, (uint16_t)y1, DMDRVI_INPUT_CONTACT_UP);
    settle(s, 3 * STEP_MS);
    touch(s, 0, 0, DMDRVI_INPUT_CONTACT_UP + 1);
    settle(s, 3 * STEP_MS);
}

static void click(shot_t* s, uint16_t x, uint16_t y)
{
    touch(s, x, y, DMDRVI_INPUT_CONTACT_DOWN);
    settle(s, 3 * STEP_MS);
    touch(s, x, y, DMDRVI_INPUT_CONTACT_UP);
    settle(s, 3 * STEP_MS);
    touch(s, 0, 0, DMDRVI_INPUT_CONTACT_UP + 1);                /* nothing touches */
    settle(s, 3 * STEP_MS);
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        Dmod_Printf("Usage: dmvshot <view.dmv> <out directory> [shot:<name> | click:<x>,<y> | wait:<ms>]...\n");
        return 1;
    }
    static shot_t s;
    memset(&s, 0, sizeof(s));
    s.out = argv[2];
    s.threshold = 128U;
    s.surface = (libdmview_surface_t){ s.pixels, SCREEN_W, SCREEN_H, SCREEN_W * 2U, DMDRVI_GFX_PIXEL_FORMAT_RGB565 };

    libdmview_display_t display = libdmview_display_register("shot");
    int status = 0;
    if (display == NULL || libdmview_claim("shot", argv[1], &s.claim) != 0)
    {
        Dmod_Printf("dmvshot: cannot claim the display\n");
        return 2;
    }
    s.view = libdmview_open(argv[1], &status);
    if (s.view == NULL)
    {
        Dmod_Printf("dmvshot: cannot open %s (%d)\n", argv[1], status);
        return 2;
    }
    (void)libdmview_attach(s.view, display);
    phoneui_backend_t backend = phoneui_backend_new();
    s.ui = ui_new(s.claim, backend);
    ui_show_all(s.ui);
    settle(&s, 200);

    int failed = 0;
    for (int i = 3; i < argc; i++)
    {
        const char* step = argv[i];
        if (strncmp(step, "shot:", 5) == 0)
        {
            failed += write_ppm(&s, step + 5, false) != 0;
            failed += write_ppm(&s, step + 5, true) != 0;
        }
        else if (strncmp(step, "click:", 6) == 0)
        {
            const char* end;
            long x = number(step + 6, &end);
            long y = (*end == ',') ? number(end + 1, NULL) : 0;
            click(&s, (uint16_t)x, (uint16_t)y);
        }
        else if (strncmp(step, "swipe:", 6) == 0)
        {
            const char* end = step + 6;
            long v[4];
            for (int k = 0; k < 4; k++)
            {
                v[k] = number(end, &end);
                if (*end == ',')
                    end++;
            }
            swipe(&s, v[0], v[1], v[2], v[3]);
        }
        else if (strncmp(step, "emit:", 5) == 0)
        {
            /* An event as if the claim's view (or libdmview_key) sent it: emit:<name>:<value> */
            libdmview_event_t ev;
            memset(&ev, 0, sizeof(ev));
            size_t n = 0;
            while (step[5 + n] != '\0' && step[5 + n] != ':' && n < sizeof(ev.name) - 1U)
            {
                ev.name[n] = step[5 + n];
                n++;
            }
            ev.value = (step[5 + n] == ':') ? (int32_t)number(step + 5 + n + 1, NULL) : 0;
            ui_handle(s.ui, &ev);
            settle(&s, 300);
        }
        else if (strncmp(step, "expect:", 7) == 0)
        {
            const char* colon = step + 7;
            size_t length = 0;
            while (colon[length] != '\0' && colon[length] != ':')
                length++;
            bool value_given = colon[length] == ':';
            bool ok = strlen(s.last) == length && strncmp(s.last, colon, length) == 0 &&
                      (!value_given || s.last_value == (int32_t)number(colon + length + 1, NULL));
            if (!ok)
            {
                Dmod_Printf("dmvshot: expected %s, the last event was %s %d\n", step + 7, s.last, (int)s.last_value);
                failed++;
            }
        }
        else if (strncmp(step, "threshold:", 10) == 0)
            s.threshold = (uint32_t)number(step + 10, NULL);
        else if (strncmp(step, "press:", 6) == 0)
        {
            const char* end;
            long x = number(step + 6, &end);
            long y = (*end == ',') ? number(end + 1, NULL) : 0;
            touch(&s, (uint16_t)x, (uint16_t)y, DMDRVI_INPUT_CONTACT_DOWN);
            settle(&s, 3 * STEP_MS);
            s.held_x = (uint16_t)x;
            s.held_y = (uint16_t)y;
        }
        else if (strcmp(step, "release") == 0)
        {
            touch(&s, s.held_x, s.held_y, DMDRVI_INPUT_CONTACT_UP);
            settle(&s, 3 * STEP_MS);
            touch(&s, 0, 0, DMDRVI_INPUT_CONTACT_UP + 1);
            settle(&s, 3 * STEP_MS);
        }
        else if (strncmp(step, "wait:", 5) == 0)
            settle(&s, (uint32_t)number(step + 5, NULL));
        else
        {
            Dmod_Printf("dmvshot: unknown step %s\n", step);
            failed++;
        }
    }
    if (ui_failures(s.ui) != 0)
    {
        Dmod_Printf("dmvshot: %u properties the view does not take\n", (unsigned)ui_failures(s.ui));
        failed++;
    }
    ui_free(s.ui);
    phoneui_backend_free(backend);
    libdmview_close(s.view);
    (void)libdmview_release(s.claim);
    libdmview_display_unregister(display);
    return failed;
}
