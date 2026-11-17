#ifndef PHONEUI_NOTES_H
#define PHONEUI_NOTES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "phoneui_backend.h"

/*
 * The notifications the phone shows: what the modules of the system notify about through the notification node (dmnotify, /dev/notify -
 * notify_sink.c subscribes to it and puts here what it is told). This file is only the list - what is active, the newest first - and
 * knows nothing of the node, so it is tested on a PC.
 *
 * phoneui_notes_put() and phoneui_notes_remove() are called in the thread of whoever notifies (from the handlers of the subscription),
 * the rest in the thread of the interface: the list has a lock, and the two that are called from outside do as little as they can
 * (they copy the texts; no formatting, no time).
 */

#define PHONEUI_NOTES           8U      /**< Notifications kept; one more drops the oldest */
#define PHONEUI_NOTE_SOURCE     24U     /**< Longest name of a source, with the end */

/** Make the list ready (once, before anything else); false: no memory for its lock */
bool phoneui_notes_init(void);
void phoneui_notes_free(void);

/** A notification of `source` on `channel` came: it replaces the one of the same source and channel, or is added as the newest */
void phoneui_notes_put(const char* source, uint32_t channel, const char* brief, const char* message);
/** The notification of `source` on `channel` is not valid anymore */
void phoneui_notes_remove(const char* source, uint32_t channel);
void phoneui_notes_clear(void);

/** Changes with every change of the list: the interface shows the rows again when it differs from what it has seen */
uint32_t phoneui_notes_serial(void);
size_t phoneui_notes_count(void);
/**
 * The rows of the home screen, the newest first (the rest of the rows are empty). `clock` ("09:41") is the time the notifications that
 * were not shown yet are stamped with - they came since the last call.
 */
void phoneui_notes_rows(phoneui_row_t rows[PHONEUI_NOTIFICATIONS], const char* clock);
/** The source of a row (the module that notified - the application a tap on the row opens) and its channel (what the application is told to open); false: the row is empty */
bool phoneui_notes_source(unsigned row, char* source, size_t size, uint32_t* channel);

/** What a source is shown as (the icon of the row): phoneui_kind_t */
int32_t phoneui_notes_kind(const char* source);

#endif
