# The contract between the view and the application

A view (`screens/<screen>/index.html`) and the application (`src/ui.c`) talk through the channel of the claim (`dmod.prop` / `dmod.get` /
`dmod.onchange` / `dmod.emit` in the page, `libdmview_prop_set_*` / `libdmview_wait_event` in the application). **Every screen has the same
properties and emits the same events** - what a screen does not show it ignores. The names are the contract; the application is written
against them once.

Only a change is sent (the application remembers what it set): on an e-paper every change of a property is a redraw, so a refresh.
Texts are at most 48 bytes (a property holds 64).

## Properties set by the application

| Property | Type | The view shows |
|----------|------|----------------|
| `clock` | text `"09:41"` | the status bar and the big clock of the home screen |
| `date` | text | the date under the clock |
| `home.line` | text | the line under the date (the mesh, the GPS) |
| `search.text`, `search.on` | text, 0/1 | what the keyboard wrote and whether the search bar of the home page shows it (else its hint) |
| `battery` | 0..100 | the number and one of five icons in the status bar |
| `charging` | 0/1 | the charging icon instead of the battery's |
| `signal` | 0..4 | the bars of the cellular network |
| `wifi`, `bt`, `gps`, `lora` | 0/1 | the indicator is shown (`wifi`: connected to a network) |
| `toast` | text | a line at the bottom for a moment. The text must change for the view to see it (the application alternates a trailing space) |
| `n<0..2>.kind` | 0 none, 1 sms, 2 mesh, 3 call, 4 chat | a row of the home screen's notifications and its icon (set last: the row appears with its texts) |
| `n<i>.title`, `.text`, `.time` | text | its texts |
| `n<i>.badge` | int | unread messages, 0: no badge |
| `m<0..4>.kind` | as above | a row of the inbox and the tag (SMS, MESH, CALL, CHAT); 0: no row |
| `m<i>.initial`, `.title`, `.text`, `.time` | text | the avatar's letter and the texts |
| `m<i>.badge` | int | unread messages: the number, the row's avatar and name inverted / bold |
| `chat.title`, `chat.sub`, `chat.ini`, `chat.stamp`, `chat.state` | text | the header of the open conversation, the time line and the delivery of the last own message |
| `c<0..3>.side` | 0 none, 1 the other party, 2 own | a bubble of the conversation |
| `c<i>.l1`, `c<i>.l2` | text | its two lines (the application wraps) |
| `q0` .. `q8` | 0/1 | the quick settings tiles, in the order Wi-Fi, 4G, Bluetooth, LoRa, GPS, airplane, backlight, vibrate, DND |
| `q.level` | 0..10 | the backlight's segments |
| `q.battery` | text | the line under them |
| `store.kicker`, `store.title`, `store.text` | text | the banner of the store |
| `s<0..2>.kind` | 0/1 | a row of the store |
| `s<i>.title`, `.sub`, `.button` | text | its texts; the button says GET, OPEN or UPDATE |

## Events emitted by the view

| Event | Value | When |
|-------|-------|------|
| `page` | 0 home, 1 apps, 2 inbox, 3 chat, 4 quick settings, 5 store | a page opened (the application refreshes what it shows) |
| `launch` | the application's number (0..23, the order of the list) | an application's tile or a dock tile was touched (the pages that exist - the inbox, the store - open by themselves) |
| `notification` | the row | a notification of the home screen |
| `apps.filter` | 0 all, 1 comms, 2 tools, 3 media, 4 more | a filter chip of the list |
| `char` | Unicode | a key that gives a character went down (from libdmview_key): the application writes it in the search bar |
| `key` | USB HID usage | a key that gives none went down: Backspace erases, Escape clears, Enter takes the text (a toast) |
| `mods` | bits | the modifiers changed (shift 1, ctrl 2, alt 4, sym 8, caps 16) - not used yet |
| `inbox.tab` | 0 all, 1 sms, 2 mesh, 3 chats | a tab of the inbox: the application sets the rows of the filtered list |
| `thread` | the row of the filtered list | a conversation was opened (the view opens its page) |
| `compose`, `call`, `send`, `search`, `banner` | 0 | the buttons of that name |
| `quick` | the tile 0..8 | a tile of the quick settings: the application does it and sets `q<i>` (tile 0 switches the Wi-Fi radio; when it cannot, the `toast` says so) |
| `level` | 1..10 | a segment of the backlight bar |
| `store.tab` | 0 featured, 1 apps, 2 updates | a tab of the store: the application sets the rows |
| `store.action` | the row | the button of a row of the store |

## What the view does by itself

Moving between its pages, the look of the tabs and chips, hiding and showing what a property says. Nothing that needs the phone.
