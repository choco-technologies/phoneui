# The page for an e-paper (`epaper-240x320`)

The display is 240 x 320, black and white, touched. A refresh of the whole screen takes about a second, of an area 0.7 s: the page is made
for that, and for what the converter (`todmvs`: `dmvs_html` + `dmvs_js`) can do.

## Design

- **No animation, no gray.** A change of state is one redraw of the rectangle that changed. Lines are 2 px, state is inversion (a black
  tile is on). Text is at least 10 px and in the weight 600 (700 white on black): the display makes its 1-bit picture of what libdmview draws
  by luminance (`threshold`, 128 in the board's `display.ini`), and the thin strokes of a lighter weight fall out.
- **Targets of 36 px and more**, and three buttons that are always there: back, home, applications. The status bar opens the quick settings.
- **A pressed button is inverted** (`:active` in `ui.css`, which the converter turns into the look of `$box.pressed`): an e-paper has no hover and
  shows a touch only when it is redrawn, so the feedback comes with the first partial refresh (0.7 s). Rows and tiles turn black, an inverted
  (selected, on) one turns white. A layered widget (chip, tab, tile) cannot do it with opacity - the converter handles a pressed look of colors and
  backgrounds only - so its root and its visible layer swap colors.
- **The keyboard first** (the second step): the search bar is the way to anything the grid does not show.

## What the converter does not do - and the page that works

These were found by building this page; each has a reason in `dmvs_html`'s limits:

- **Scripts are inline.** `<script src>` is not read (only the `dmod.js` stand-in is left out on purpose). `index.html` holds the script.
- **Handlers are `onclick = ...` in the script, not attributes**: a page may have 32 *programs* (every `onclick="..."` is one) and 256 clicks.
- **Hide by opacity, not `display: none`.** An element that a script changes has to be laid out when the page is converted: `.hidden` and an
  inactive `.page` are `opacity: 0; pointer-events: none`. A change of a class of an element that has no box crashes the converter.
- **A class may change opacity, not how an element looks.** The weight of a font or a background changing with a class is painted in
  every state without a condition. Everything that changes is a **layer** shown by its opacity (`.lyr`, `.on`): an inverted tile is a black
  layer over its outline and two texts (black, white), of which the property shows one.
- **A text a property sets needs a text to start with and a width.** An element empty in the page gets no `TEXT`; one with a text gets a box
  as wide as that text. Every runtime text has a sample text (the stub's) and a fixed width; texts are single-line (a bubble has two).
- **A box centred in its parent is drawn centred.** An element whose box is symmetric in its parent gets `CENTER` for a text variable (a
  heuristic of the converter). The text lines of a bubble are 160 px in a 186 px content box to be left-aligned.
- **A table of pointers needs relocations.** Not about the page: the stub keeps its texts inside its arrays.
- **Fonts.** Every character of the script's source (and the digits) is in the font, so `CHARS` of `index.html` lists what the application
  may send - Polish letters included. The big clock's font (46 px, all of them) is 47 kB.

## The pictures

`dmvshot` ([README](../README.md#seeing-it-on-a-pc-dmvshot)) draws any page on a PC, with the real application logic; ctest runs a tour of the
pages (`dmvshot_tour_*`) and fails when a touch opens the wrong page or the application sets a property the view does not have.
