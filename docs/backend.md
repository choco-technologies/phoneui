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
| `..._launch`, `..._request` | "not available yet" | the applications, the keyboard (search, send) |

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
