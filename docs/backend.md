# The backend

`src/phoneui_backend.h` is what the interface needs of the phone: the state it shows (the status, the notifications, the inbox, a
conversation, the quick settings, the store) and what it asks for (launching an application, toggling a setting, a store action). The
functions are plain C, so a real function replaces its part of the stub (`src/backend_stub.c`) without touching the application (`src/ui.c`)
or the view:

| Part | Stub today | Plugged in by |
|------|------------|---------------|
| `phoneui_backend_status` | made-up status, a clock from the uptime | the RTC (dmutc), the battery gauge (BQ27220), the modem, the GPS |
| `..._notifications`, `..._inbox`, `..._thread` | made-up conversations | the message store: SMS (modem), the mesh (LoRa), messengers |
| `..._quick*` | the tiles keep what they were set to; **the Wi-Fi tile is real** (below) | the other radios, the backlight, the haptic motor, the settings |
| `..._store*` | made-up applications, "installs" nothing | the registry and `dmf-get` |
| `..._apps` | **real** (below) | |
| `..._request` | "not available yet" | what the dock and the buttons ask for: calls, messages, search, send |

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

Texts are cut to fit the view's fields (48 bytes). A table of the stub keeps its texts inside its arrays: a table of pointers to
texts needs the relocations of the module, which a small embedded loader does not always do.
