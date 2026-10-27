# PC Settings

The game's own Options menu has a **PC Settings** entry (retail 01.21 only). It opens
native pages built from the game's menu templates, in the game's language:

| Page | Settings |
|---|---|
| Display | resolution (as in the launcher, the monitor's native one, or 720p to 2160p), display mode (windowed, borderless window, exclusive fullscreen), V-Sync, frame rate limit, refresh rate in exclusive fullscreen, 60 FPS mode |
| Upscaling | method (off, AMD FSR 1, AMD FSR 3, NVIDIA DLSS), mode (Native 100%, Quality 67%, Balanced 58%, Performance 50%, Ultra performance 33%), sharpness (0-100%) |
| Geometry and textures | texture quality, anisotropic filtering, shadow quality (off, low, medium, high, ultra), kart and driver detail, scenery detail, view distance, effects quality |
| Effects and lighting | ambient occlusion, character shadows, bloom, motion blur, depth of field, fur detail |
| Quality presets | Original, Low, Medium, High, Ultra |

The launcher shows the same pages in the GRAPHICS section of its Settings tab (scroll with the
mouse wheel); a choice there is saved at once to the same file.

The upscaling mode is a share of the output resolution: the 3D scene is rendered at that share
of the Resolution height and reconstructed by the chosen method (with no method the scene is
rendered at the output resolution). NVIDIA DLSS in Native mode is DLAA. Each method and level
appears only where the renderer provides it: with Direct3D 11 the methods are Off and AMD FSR 1
and Ultra shadows are available (shadow maps at twice the PS4 size); with Vulkan, FSR 3 and DLSS.
Sharpness drives FSR 1 RCAS (0% turns it off) and FSR 3; DLSS has no sharpness control.

Shadow quality sets the shadow controls of earlier versions: Off = no cascaded shadows, Low = 50%
resolution and 2 cascades, Medium = 75% and 3 cascades, High = the original shadows, Ultra = High
with Direct3D 11 shadow maps at 2x. Settings files of earlier versions are converted when read:
the internal 3D resolution becomes the nearest upscaling mode, the four shadow settings the
nearest shadow quality, FSR 1 sharpness a percentage.

Settings marked "(restart)" apply at the next start of the game; the others apply at
once. Graphics page settings write the same values as the game's own graphics debug
options and "Original" leaves the game's value untouched. Anti-aliasing is not offered:
turning FXAA off or switching to SMAA left the 3D scene black on every renderer and game
version, so an older stored value is ignored and the game keeps its FXAA. Saving happens in the background and never blocks the menu; choices are
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

## Other game versions (US CUSA13795, v01.00)

The settings were written against the EU 01.21 eboot (CUSA14876). At start the runtime looks for
the same code in the eboot that runs (`../retail_map.hpp`, signatures in `../retail_anchors.hpp`,
made by `tools/retail_anchors.py` from the EU 01.21 eboot): each binding is a window of EU code
around it, with the bytes that change when code moves left out, and it is found only when that
window occurs exactly once. Every setting then checks its own instructions there before
changing them, as on EU 01.21. A setting whose code is not found stays original and the log
says so (`settings: ... not available on this eboot`); without the menu's selector code the PC
Settings menu is off. The log also lists the bindings that were not found and compares three of
them with offsets recorded for the US eboots by other means (`retail_map: check ...`).

Checked on EU 01.00, which differs from 01.21 much more than the US 01.21 eboot does: all 7
offsets already known there by other means were found at the right place and none at a wrong
one; 21 of the 32 bindings were found, the others belong to features 01.00 does not have (the
menu selector, the Wumpa shop).

## If the game does not start after a change

Close the game and the launcher, open a command prompt in the installation folder and
run:

    build\CTRPort-gpu\ctrport.exe --settings-reset-graphics runtime\user\launcher_config.json

It prints `SETTINGS_GRAPHICS_RESET` and restores the graphics settings without starting
the game, keeps the previous file as `.bak` and leaves game paths, saves and other
settings alone.
