# PC Settings

The game's own Options menu has a **PC Settings** entry (retail 01.21 only). It opens
native pages built from the game's menu templates, in the game's language:

| Page | Settings |
|---|---|
| Display | 60 FPS, fullscreen, window size, window mode, refresh rate, vertical sync, frame limit |
| Graphics | anti-aliasing (FXAA, off, SMAA), bloom, motion blur, depth of field, fur detail, view distance |
| Resolution and upscaling | render resolution, upscaler (FSR 1, FSR 3, DLSS), FSR 1 sharpness |
| Quality | texture filtering, texture quality, kart and driver detail, scenery detail |
| Shadows and effects | shadows, shadow resolution, shadow cascades, effects quality |
| Quality presets | Original, Low, Medium, High, Ultra |

Settings marked "(restart)" apply at the next start of the game; the others apply at
once. Graphics page settings write the same values as the game's own graphics debug
options and "Original" leaves the game's value untouched. SMAA may not engage with
PS4 Pro mode. Saving happens in the background and never blocks the menu; choices are
stored in `runtime/user/launcher_config.json`, the same file the launcher uses.

## How the menu is made

Nothing from the game is shipped. At the first start the runtime builds the menu in
memory from your own retail 01.21 archives (about a tenth of a second) and serves it
to the game as part of `archives/juicedomain_permanent.pak`, `archives/update.pak` and
`archives/update2.pak`. The files on disk are only read and never copied or changed.
Every menu template is checked against the retail 01.21 contents: with another game
version, or a mod that replaces those archives, the menu is left out and the game
reads its files unchanged. Set `CTR_PC_MENU=0` to start without it.

Sources: `menu_assets.hpp` (menu resources), `menu_overlay.hpp` (served archives),
`menu_catalog.hpp` (entries and translations, generated from the development
catalog), `menu_bridge/` (selector events and saving), `native_graphics.hpp` (Graphics
page). `menu_assets_control.cpp` builds the menu from given archives and, with a
reference folder, checks it byte for byte. The per-component READMEs describe checks
that run in the development repository against private extracts of the user's dump;
those tools are not part of this repository.

## If the game does not start after a change

Close the game and the launcher, open a command prompt in the installation folder and
run:

    build\CTRPort-gpu\ctrport.exe --settings-reset-graphics runtime\user\launcher_config.json

It prints `SETTINGS_GRAPHICS_RESET` and restores the graphics settings without starting
the game, keeps the previous file as `.bak` and leaves game paths, saves and other
settings alone.
