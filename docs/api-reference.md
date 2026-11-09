# phoneui

## Usage

```text
phoneui [<view>] [<display>]
```

| Argument | |
|----------|-|
| `view` | the `.dmv` to show; default `/eviews/phoneui_view/phoneui.dmv` (dmod-boot: the views of the flash modules) |
| `display` | the name of the display to claim; default the first one dmview serves |

It waits for the view and for the display, claims the display (`libdmview_claim()`), shows what its backend says and runs the events of the
view until it is stopped (`libsystemd`: `configs/phoneui.ini`). Exit codes: 0, `-ENOENT` (no view), `-ENODEV`/other (the claim), `-ENOMEM`.

The properties and the events: [contract.md](contract.md). The functions behind: [backend.md](backend.md).

## dmvshot (host)

```text
dmvshot <view.dmv> <out directory> [shot:<name> | click:<x>,<y> | wait:<ms> | expect:<event>[:<value>] | threshold:<n>]...
```

Draws the view with the real `src/ui.c` and the stub backend into an off-screen RGB565 buffer; `shot:` writes `<name>.ppm` (what libdmview
drew) and `<name>-mono.ppm` (the e-paper's picture of it, white from the luminance `threshold`). The exit code is the number of failed
expectations, and of properties the view does not take.
