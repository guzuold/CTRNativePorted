# Xbox prompts and controller exit

The launcher Settings card has **Icone Xbox colorate**. Disable it to retain the
original PS4 symbols. Command-line runs use `CTR_XBOX_PROMPTS=1` (unset/0 = PS4).

The launcher prepares the archives itself, without Python, the first time PLAY is
pressed with the option on (`ctrport/xbox_prepare.hpp`, about 10 s for v01.21; the
button shows ATTENDI meanwhile). It reads the same archives the runtime mounts
(patch first, then base), reads the PNGs from `assets/xbox-default` next to
`launcher.exe` (release) or `ctrport/assets/xbox-default` (source tree), and keeps
`runtime/user/mods/xbox-default/sources.cache` (path, size, write time -> source
hash) so later launches do not rehash the retail archives. On failure it shows the
error and does not start the game; disabling the option falls back to PS4 symbols.
**A release must ship `assets/xbox-default/` (the CC0 PNGs, LICENSE.txt,
provenance.json)**; v0.2.0 did not, and needed the Python tool below.

The native port decodes the retail LZMA blocks with its own decoder: the six
extracted fonts match the SHA256 values that `ctr_nf_iga.py` produces
(`tools/input_prompts/xbox_prepare_test.cpp extract`). Icons are resized with GDI+
bicubic instead of Pillow Lanczos, so the patched atlas differs from the Python
output only inside the glyph rectangles (37,246 of 10,043,828 bytes for
GeekSpeak_Outline); a side-by-side render of the two atlases was inspected.

Equivalent manual preparation with Python, using the actual retail files selected by
the patch-over-base mount:

```powershell
python -B tools/input_prompts/prepare_xbox.py <retail-directory>/archives/permanent.pak
python -B tools/input_prompts/prepare_xbox.py <retail-directory>/archives/languagestartup.pak
python -B tools/input_prompts/prepare_xbox.py <patch-directory>/archives/update.pak
```

The tool needs Pillow. It builds a local archive under
`runtime/user/mods/xbox-default/<source SHA256>/<archive name>`; nothing is written
to the retail dump. The runtime selects it only when Xbox prompts are enabled,
the source archive hash matches, and the generated archive checksum passes.
Unknown atlases fail preparation; missing or damaged overrides fail explicitly.
The launcher and runtime do not need Python to use a prepared archive.

Ten glyphs in each of the retail `fonts/GeekSpeak_Outline.igz` and
`fonts/Zoinks_Outline.igz` button atlases, plus `fonts/GeekSpeak.igz` in
`languagestartup.pak`, are replaced:
Cross=A, Circle=B, Square=X, Triangle=Y, L1=LB, R1=RB, L2=LT, R2=RT, and the two
stick icons. Original glyph metrics and three non-controller symbols remain.
This does not replace a controller illustration in the controls diagram, nor
claim to replace symbols baked into videos or other textures.

The PNGs are Julio Cacko's Default icons, with colored face buttons (CC0-1.0).
Only third-party PNGs, provenance, hashes and conversion code are distributable;
the generated archive and preview contain retail material and stay in ignored
`runtime/user/`.

## Controller exit

On the first XInput controller (the same port used by the game), hold **View +
Menu** for 1.2 seconds. Release all buttons, then **A** confirms exit or **B**
cancels. The host confirmation also has clickable buttons. Loss of focus or
disconnection cancels; holding A during activation does not confirm. The runtime
consumes controller input while the confirmation is active, including the button
release after cancellation. The game itself continues running and is not paused.

Exit uses the existing window-close path (database close followed by `_Exit`).
It is not a reconstructed engine shutdown, and does not wait for save writes;
the confirmation asks the player to wait for saving to finish.

## Verification

`tools/input_prompts/control_test.cpp` covers hold timing, release-before-confirm,
cancel, draining held buttons, focus loss, disconnect, archive routing and PS4
fallback. `prepare_xbox.py` rereads its output through the existing archive
reader, verifies the replacement and unchanged other entry descriptors, and
checks that the original file hash is unchanged. The atlas was visually inspected.
These checks do not substitute for a screenshot and controller test in the game.

On 2026-10-14 the user verified the physical controller sequence: open, B to
cancel, reopen, A to exit. The runtime logged all four events and exited with 0
(`play_20261014_231516`). The first visual pass revealed that the introduction
uses the language-startup font as well; preparation now replaces all three
matching button atlases. Retail pixels are BGRA8; color conversion preserves
the intended Xbox red B, blue X, yellow Y and green A.

The v01.21 update.pak overrides all three fonts. Its two distinct atlas hashes
and 14-entry metrics are handled separately from v01.00; the additional touchpad
glyph becomes View, matching the runtime input mapping. Preparing the update
creates a local copy of approximately 1.7 GB; the retail update is unchanged.

Final retail v01.21 visual check: `runtime/user/captures/xbox_121.png` shows
green Xbox A in the start-screen prompt; `play_20261014_232339` logs the
three archive overrides. No exhaustive menu/track prompt coverage is claimed.
