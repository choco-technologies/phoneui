#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmnotify.h"
#include "dmnotify_ioctl.h"
#include "notify_sink.h"
#include "notes.h"

#define NOTIFY_NODE     "/dev/notify"
#define SUBSCRIBER      "phoneui"           /* the name of this module: dmnotify calls the functions below by it */

static void* g_node;                        /* the open file of the node: the subscription lasts as long as it is open */

/*
 * Called by dmnotify in the thread of the module that notifies: only the copy into the list. (No lock screen yet, so the privacy of a
 * notification changes nothing here: the home screen is shown to whoever unlocked the phone.)
 */
dmod_dmnotify_dif_api_declaration(1.0, phoneui, void, _on_notify, ( const dmnotify_notification_t* notification ))
{
    if (notification != NULL)
        phoneui_notes_put(notification->source, notification->channel, notification->brief, notification->message);
}

dmod_dmnotify_dif_api_declaration(1.0, phoneui, void, _on_cancel, ( const dmnotify_notification_t* notification ))
{
    if (notification != NULL)
        phoneui_notes_remove(notification->source, notification->channel);
}

bool phoneui_notify_open(void)
{
    if (g_node != NULL)
        return true;
    if (!phoneui_notes_init())
        return false;
    const char* path = Dmod_GetEnv("PHONEUI_NOTIFY");
    void* node = Dmod_FileOpen((path != NULL && path[0] != '\0') ? path : NOTIFY_NODE, "r");
    if (node == NULL)
        return false;
    dmnotify_subscribe_t subscribe;
    subscribe.module_name = SUBSCRIBER;
    int ret = Dmod_Ioctl(node, DMNOTIFY_IOCTL_CMD_SUBSCRIBE, &subscribe);     /* (what is active now comes before this returns) */
    if (ret != 0)
    {
        DMOD_LOG_ERROR("phoneui: cannot subscribe to the notifications (%d)\n", ret);
        Dmod_FileClose(node);
        return false;
    }
    g_node = node;
    return true;
}

void phoneui_notify_close(void)
{
    if (g_node != NULL)
        Dmod_FileClose(g_node);             /* the subscription ends with the file */
    g_node = NULL;
    phoneui_notes_clear();
}
