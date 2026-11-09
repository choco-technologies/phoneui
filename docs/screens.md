# Screens

A **screen** is a kind of display the interface is designed for: its size, its technology, how it is touched. Each has a page of its own
(`screens/<id>/index.html` with its style sheet), because a 240 x 320 e-paper and a 480 x 272 color display are not the same interface
resized. The logic is shared: every page follows the [contract](contract.md), so `phoneui` (the application) does not know which one is shown.

| Screen id | Display | State |
|-----------|---------|-------|
| `epaper-240x320` | the 3.1" e-paper of the LILYGO T-Deck Pro, black and white, touch (+ a keyboard to come) | all the pages |

The id is an opaque label for `dmf-get` (`--screen <id>`, `@screen <id>` in a `.dmd`): it is matched literally, nothing is read from it.

## Adding a screen

1. Make `screens/<id>/index.html` (+ `ui.css`): the element `phoneui-screen` is the screen. Use the properties and the events of the
   [contract](contract.md); a screen that does not show one just leaves it out. Read the notes of [design-epaper.md](design-epaper.md): the
   converter has limits.
2. Add the id to `PHONEUI_SCREENS` and its size to `PHONEUI_SCREEN_SIZE_<id>` in `CMakeLists.txt`.
3. Add a `views=./views/<id> => ${views_dir} [screen=<id>]` line to `phoneui_view.dmr` (`[default=true]` on one of them).
4. Select it in the board's `flash.dmd`: `phoneui_view @screen <id>`.

Look at it on a PC with [dmvshot](../README.md#seeing-it-on-a-pc-dmvshot) before it goes to a board.
