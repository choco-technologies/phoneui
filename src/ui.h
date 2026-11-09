#ifndef PHONEUI_UI_H
#define PHONEUI_UI_H

#include <stdint.h>
#include "libdmview.h"
#include "phoneui_backend.h"

/*
 * The logic of the application between the view and the backend: it sets the view's properties from what the backend says (only a
 * change is sent) and runs the events of the view. The service (phoneui.c) and the preview tool (tools/dmvshot) both use it.
 */

typedef struct ui* ui_t;

/** @param claim  the claim of the display the view is shown on (libdmview_claim) */
ui_t ui_new(uint32_t claim, phoneui_backend_t backend);
void ui_free(ui_t ui);
/** Properties the view did not take (it has none of that name): the contract is broken */
uint32_t ui_failures(ui_t ui);

/** Everything the view shows, from the backend */
void ui_show_all(ui_t ui);
/** The status bar and the home screen (the clock moves) */
void ui_show_status(ui_t ui);
/** What the view asked for */
void ui_handle(ui_t ui, const libdmview_event_t* event);

#endif /* PHONEUI_UI_H */
