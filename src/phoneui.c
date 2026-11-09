#include "dmod.h"
#include "dmosi.h"
#include "libdmview.h"
#include "libsystemd.h"
#include "phoneui_backend.h"
#include "ui.h"
#include <string.h>
#include <errno.h>

/**
 * @brief phoneui - the graphical interface of the phone: the application that shows its view and runs what the view asks for.
 *
 * The screens are a view (phoneui_view, one bundle for each kind of display; docs/screens.md): pages, buttons and their looks are in it,
 * and run in libdmview. phoneui claims the display for the view (libdmview_claim()) and talks to it through the channel of the claim:
 * it sets the view's properties - the clock, the battery, the rows of the lists ... (docs/contract.md) - and takes the events - what
 * was touched. What the phone does comes from a backend (phoneui_backend.h): until the functions are there, the stub.
 *
 * It is started as a service (configs/phoneui.ini): the display is waited for.
 */

#define DEFAULT_VIEW        "/eviews/phoneui_view/phoneui.dmv"      /* dmod-boot: the views of the flash modules */
#define DISPLAY_WAIT_MS     10000u
#define VIEW_WAIT_MS        10000u
#define WAIT_MS             100u
#define EVENT_WAIT_MS       500u            /* The clock and the state of the phone are looked at this often */
static bool wait_for(const char* path, dmosi_semaphore_t wakeup, uint32_t timeout_ms)
{
    for (uint32_t waited = 0; !libsystemd_stop_requested(); waited += WAIT_MS)
    {
        if (Dmod_FileAvailable(path))
            return true;
        if (waited >= timeout_ms)
            break;
        (void)dmosi_semaphore_wait(wakeup, 1, (int32_t)WAIT_MS);
    }
    return false;
}

int main(int argc, char* argv[])
{
    const char* view = (argc > 1 && argv[1][0] != '\0') ? argv[1] : DEFAULT_VIEW;
    const char* display = (argc > 2 && argv[2][0] != '\0') ? argv[2] : NULL;

    dmosi_semaphore_t wakeup = dmosi_semaphore_create(0, 1);
    if (wakeup == NULL)
        return -ENOMEM;
    libsystemd_set_stop_semaphore(wakeup);

    if (!wait_for(view, wakeup, VIEW_WAIT_MS))
    {
        DMOD_LOG_ERROR("phoneui: no view %s\n", view);
        Dmod_Printf("Usage: phoneui [<view>] [<display>]\n");
        return -ENOENT;
    }

    uint32_t claim = 0;
    int ret = -ENODEV;
    for (uint32_t waited = 0; !libsystemd_stop_requested(); waited += WAIT_MS)
    {
        ret = libdmview_claim(display, view, &claim);
        if (ret != -ENODEV || waited >= DISPLAY_WAIT_MS)
            break;
        (void)dmosi_semaphore_wait(wakeup, 1, (int32_t)WAIT_MS);
    }
    if (ret != 0)
    {
        DMOD_LOG_ERROR("phoneui: cannot claim %s (%d)\n", display != NULL ? display : "a display", ret);
        return ret;
    }

    phoneui_backend_t backend = phoneui_backend_new();
    ui_t ui = ui_new(claim, backend);
    if (backend == NULL || ui == NULL)
    {
        phoneui_backend_free(backend);
        ui_free(ui);
        (void)libdmview_release(claim);
        return -ENOMEM;
    }
    DMOD_LOG_INFO("phoneui: showing %s\n", view);
    ui_show_all(ui);
    while (!libsystemd_stop_requested())
    {
        libdmview_event_t ev;
        int got = libdmview_wait_event(claim, &ev, (int32_t)EVENT_WAIT_MS);
        if (got == -ENOENT)
            break;                                              /* the claim is gone - the display with it */
        if (got == 0)
            ui_handle(ui, &ev);
        ui_show_status(ui);                                     /* the clock moves */
    }
    ui_free(ui);
    phoneui_backend_free(backend);
    (void)libdmview_release(claim);
    return 0;
}
