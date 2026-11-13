#ifndef PHONEUI_BACKEND_H
#define PHONEUI_BACKEND_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * What the phone does, as the interface shows it. phoneui (the application) asks a backend for the state the screens show and tells
 * it what the user did; the view knows nothing of it (it has properties and events - docs/contract.md). The implementation of this
 * file is the stub (backend_stub.c): data made up, nothing really done. A real function replaces its part of the stub - the
 * telephony, the messages, the radio, the settings, the store - without touching the application or the view. Real so far: the Wi-Fi
 * (the tile of the quick settings and the icon of the status bar).
 */

#define PHONEUI_TEXT            48U     /**< Longest text of a field of the view (it has 64 bytes with the end) */
#define PHONEUI_NOTIFICATIONS   3U      /**< Rows of the home screen */
#define PHONEUI_THREADS         5U      /**< Rows of the inbox */
#define PHONEUI_BUBBLES         4U      /**< Messages of an open conversation */
#define PHONEUI_STORE_ROWS      3U
#define PHONEUI_QUICK           9U      /**< Tiles of the quick settings */
#define PHONEUI_APPS            24U     /**< Applications of the list (two pages) */
#define PHONEUI_LEVELS          10U     /**< Steps of the backlight */

/** What a row of the lists is about (the view draws the icon or the tag by it); 0: the row is empty */
typedef enum
{
    phoneui_kind_none = 0,
    phoneui_kind_sms,
    phoneui_kind_mesh,
    phoneui_kind_call,
    phoneui_kind_chat,
} phoneui_kind_t;

typedef struct
{
    char    clock[8];                   /**< "09:41" */
    char    date[PHONEUI_TEXT];         /**< "Saturday, 7 November" */
    char    line[PHONEUI_TEXT];         /**< The line under the date of the home screen */
    int32_t battery;                    /**< 0..100 */
    int32_t charging;
    int32_t signal;                     /**< 0..4 bars of the cellular network */
    int32_t wifi, bt, gps, lora;        /**< The indicators of the status bar: 1 shown (wifi: connected to a network) */
} phoneui_status_t;

/** A row of the home screen's notifications, of the inbox */
typedef struct
{
    int32_t kind;                       /**< phoneui_kind_t */
    char    initial[4];                 /**< The letter of the avatar (inbox) */
    char    title[PHONEUI_TEXT];
    char    text[PHONEUI_TEXT];
    char    time[12];
    int32_t badge;                      /**< Unread messages, 0: none */
} phoneui_row_t;

/** A message of a conversation: two lines of the bubble; side 0 - none, 1 - the other party's, 2 - own */
typedef struct
{
    int32_t side;
    char    line1[PHONEUI_TEXT];
    char    line2[PHONEUI_TEXT];
} phoneui_bubble_t;

typedef struct
{
    char             title[PHONEUI_TEXT];
    char             sub[PHONEUI_TEXT];
    char             initial[4];
    char             stamp[PHONEUI_TEXT];
    char             state[PHONEUI_TEXT];       /**< The delivery of the last own message: "✓ 09:12" */
    phoneui_bubble_t bubbles[PHONEUI_BUBBLES];
} phoneui_thread_t;

typedef struct
{
    int32_t on[PHONEUI_QUICK];          /**< 1: the tile is on (Wi-Fi: the radio is on, connected or not) */
    int32_t level;                      /**< The backlight, 0..PHONEUI_LEVELS */
    char    battery[PHONEUI_TEXT];      /**< "Battery 82%  ·  about 2 days left" */
} phoneui_quick_t;

typedef struct
{
    char    kicker[PHONEUI_TEXT], title[PHONEUI_TEXT], text[PHONEUI_TEXT];     /**< The banner */
    struct
    {
        int32_t kind;                   /**< 0: no row */
        char    title[PHONEUI_TEXT];
        char    sub[PHONEUI_TEXT];
        char    button[12];             /**< GET, OPEN or UPDATE */
    } rows[PHONEUI_STORE_ROWS];
} phoneui_store_t;

typedef struct phoneui_backend* phoneui_backend_t;

phoneui_backend_t phoneui_backend_new(void);
void phoneui_backend_free(phoneui_backend_t backend);

void phoneui_backend_status(phoneui_backend_t backend, phoneui_status_t* status);
void phoneui_backend_notifications(phoneui_backend_t backend, phoneui_row_t rows[PHONEUI_NOTIFICATIONS]);
/** The inbox filtered by tab (0 all, 1 SMS, 2 mesh, 3 chats) */
void phoneui_backend_inbox(phoneui_backend_t backend, int tab, phoneui_row_t rows[PHONEUI_THREADS]);
/** The conversation of a row of the inbox (index of the filtered list) */
void phoneui_backend_thread(phoneui_backend_t backend, int tab, int row, phoneui_thread_t* thread);
void phoneui_backend_quick(phoneui_backend_t backend, phoneui_quick_t* quick);
/** A tile of the quick settings was tapped; a message (a line for the user) only when it could not be done ("" otherwise) */
void phoneui_backend_quick_toggle(phoneui_backend_t backend, int tile, char* message, size_t size);
void phoneui_backend_quick_level(phoneui_backend_t backend, int level);
void phoneui_backend_store(phoneui_backend_t backend, int tab, phoneui_store_t* store);
/** The button of a row of the store: what it did, as a line for the user (the result of a stub says it is one) */
void phoneui_backend_store_action(phoneui_backend_t backend, int tab, int row, char* message, size_t size);
/** An application of the list; the line for the user ("not available yet" until the application exists) */
void phoneui_backend_launch(phoneui_backend_t backend, int app, char* message, size_t size);
/** Anything else the interface asks for (search, compose, call, send ...): the line for the user */
void phoneui_backend_request(phoneui_backend_t backend, const char* what, char* message, size_t size);

#endif /* PHONEUI_BACKEND_H */
