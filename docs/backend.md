# The backend

`src/phoneui_backend.h` is what the interface needs of the phone: the state it shows (the status, the notifications, the inbox, a
conversation, the quick settings, the store) and what it asks for (launching an application, toggling a setting, a store action). The
functions are plain C, so a real function replaces its part of the stub (`src/backend_stub.c`) without touching the application (`src/ui.c`)
or the view:

| Part | Stub today | Plugged in by |
|------|------------|---------------|
| `phoneui_backend_status` | made-up status, a clock from the uptime | the RTC (dmutc), the battery gauge (BQ27220), the modem, the GPS |
| `..._notifications`, `..._inbox`, `..._thread` | made-up conversations | the message store: SMS (modem), the mesh (LoRa), messengers |
| `..._quick*` | the tiles keep what they were set to | the radios, the backlight, the haptic motor, the settings |
| `..._store*` | made-up applications, "installs" nothing | the registry and `dmf-get` |
| `..._launch`, `..._request` | "not available yet" | the applications, the keyboard (search, send) |

Texts are cut to fit the view's fields (48 bytes). A table of the stub keeps its texts inside its arrays: a table of pointers to
texts needs the relocations of the module, which a small embedded loader does not always do.
