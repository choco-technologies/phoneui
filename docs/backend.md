# The backend

`src/phoneui_backend.h` is what the interface needs of the phone: the state it shows (the status, the notifications, the inbox, a
conversation, the quick settings, the store) and what it asks for (launching an application, toggling a setting, a store action). The
functions are plain C, so a real function replaces its part of the stub (`src/backend_stub.c`) without touching the application (`src/ui.c`)
or the view:

| Part | Stub today | Plugged in by |
|------|------------|---------------|
| `phoneui_backend_status` | made-up status, a clock from the uptime; **the battery is real** when the board has a gauge (below) | the RTC (dmutc), the modem, the GPS |
| `..._notifications` | **real** (below) | |
| `..._inbox`, `..._thread` | made-up conversations | the message store: SMS (modem), the mesh (LoRa), messengers |
| `..._quick*` | the tiles keep what they were set to; **the Wi-Fi tile is real** (below) | the other radios, the backlight, the haptic motor, the settings |
| `..._store*` | made-up applications, "installs" nothing | the registry and `dmf-get` |
| `..._apps` | **real** (below) | |
| `..._request` | "not available yet" | what the dock and the buttons ask for: calls, messages, search, send |

## The notifications (real)

The rows of the home screen are the notifications of the system: phoneui subscribes to the node of **dmnotify** (`/dev/notify`;
`$PHONEUI_NOTIFY` says another) with its own name and implements the DIF of dmnotify (`src/notify_sink.c`: `_on_notify`, `_on_cancel`). What the
modules notify about - libmeshcore: a text heard on a channel - lands in a list (`src/notes.c`, at most 8, the newest first), and the home screen
shows its first three rows again whenever the list changes. A board without the node has no rows; the node may also come after the display
(it is looked for until it is there).

- The row: the **brief** is the title, the **message** the text, the time is when the phone first showed it. The icon is by the **source**:
  `meshcore` the mesh, `sms`, `phone` a call, anything else a chat.
- A tap on a row starts the application named as the source of the notification (`meshcore` opens the MeshCore application), when there is one.
  The notification itself is taken back by its source (MeshCore: when the chat is on the screen).
- The handlers run in the thread of the module that notifies: they only copy the texts (no formatting, no time - `notes.c`).
- There is no lock screen yet, so the privacy of a notification changes nothing so far.

`dmvshot` (the preview on a PC) has no node: it puts three made-up notifications in the list.

## The applications (real)

The grid of the applications lists the modules that have an **icon**: the `.dmvi` files of the icons directory - `/eviews/icons`, which dmod-boot
fills with `dmf-get --icons-dir` (`$PHONEUI_ICONS` says another) - by name; a module without one is not listed (the first 24 are). The icon of a
module is made from its `icon` resource (docs of dmod: "Icons"), for the screen the image was installed for. The view draws the file by the
property `a<i>.icon` (the path without `.dmvi`), as it is - the icon of the e-paper's screen is 48 x 48, the size of the cell.

A touch on a cell **starts the application in a rent** of the screen (dmview's `docs/rent.md`): phoneui rents the area between the status bar and
the buttons (page 6 of the view is empty there), starts the module with `--display NAME` (the name of the rent) and binds the rent to its process.
The application claims that name like a display and draws into it; it has the keys while the page is shown, except Escape, which is the phone's
(it leaves the application). It ends when the rent is taken back - on any other page, on Escape, or when the user leaves - by its claim
being gone (`-ENOENT`): an application written for this ends when `libdmview_wait_event()` says so. When it ends by itself, the user is back at
the grid. A module that does not start says so in a line at the bottom.

## The Wi-Fi (real)

The first tile of the quick settings is the Wi-Fi switch of the phone, and the Wi-Fi icon of the status bar is the connection. Both are the node
of the Wi-Fi driver, `/dev/wlan0` (the interface `wlan0` of dmespwifi; where the `wifi` tool looks for it), asked with the standard commands of
dmdrvi: `DMDRVI_IOCTL_WIFI_GET_STATUS` for what is shown, `DMDRVI_IOCTL_WIFI_SET_ENABLED` for the tap.

- The tile is **on** while the radio is on (connected or not) and **off** when it is switched off.
- The icon is shown when the state is **connected**.
- A tap switches the radio. Off drops the connection and the network; on starts the radio, and what keeps the interface connected (wifid,
  from the networks saved with `wifi save`) connects again. A tap that cannot be done says why in the line at the bottom (no node: "not
  available"; the radio still starting: "try again").
- Without the node (the host: dmvshot, the tests) the tile is off, the icon is hidden and a tap says "Wi-Fi: not available".
- The state of the switch is not kept: the radio is on after a start.

## The battery (real, where the board has a gauge)

The percentage and the charging icon of the status bar, and the line under the quick settings tiles, are the node of a battery gauge, `/dev/battery`
(the name of the section of the board's ini; `PHONEUI_BATTERY` names another node), asked with the standard command of dmdrvi
`DMDRVI_IOCTL_BATTERY_GET_STATUS` (the T-Deck Pro: `dmbq27220`, the BQ27220).

- The percentage is the charge the gauge reports; the charging icon is shown while the state is **charging** (not when it is full).
- The line says what the battery does, and leaves out what the gauge cannot tell (`src/battery_line.c`): `Battery 82%  ·  4 h 10 min left` while
  discharging, `Battery 40%  ·  charging, full in 1 h 20 min`, `Battery 100%  ·  full`, `Battery 60%  ·  not charging`, `No battery`.
- Without the node (the host: dmvshot, the tests; a board without a gauge) the made-up 82 % stays, as for the rest of the stub.

Texts are cut to fit the view's fields (48 bytes). A table of the stub keeps its texts inside its arrays: a table of pointers to
texts needs the relocations of the module, which a small embedded loader does not always do.

## The clock

The clock and the date of the status bar come from `dmutc_get()` (UTC: the RTC of the board, which dmutc sets from NTP when the network is
up) in the local time: `PHONEUI_UTC_OFFSET` (minutes from UTC, default 60 = CET) and `PHONEUI_DST` (`eu`, the default: the summer time of the
European Union; `off`). Until the time is set (no RTC, no network yet) the clock says `--:--` and the date "Time not set".
