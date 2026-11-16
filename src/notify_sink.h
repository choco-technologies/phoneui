#ifndef PHONEUI_NOTIFY_SINK_H
#define PHONEUI_NOTIFY_SINK_H

#include <stdbool.h>

/*
 * The subscription of phoneui to the notifications of the system: the node of dmnotify (/dev/notify; PHONEUI_NOTIFY names another).
 * What the modules notify about lands in the list of notes.h, which the home screen shows.
 */

/** Subscribe, if not subscribed yet. false: there is no node (yet) - call again later; the phone works without it */
bool phoneui_notify_open(void);
/** End the subscription (the list is emptied) */
void phoneui_notify_close(void);

#endif
