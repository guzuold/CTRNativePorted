# Retail Options adapter

This is new project behavior around original retail callbacks. It reconstructs
no game function. It is bound to retail CUSA14876 v1.21 SHA256
`39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5`.

The retail reflection string `CGuiBehaviorOctaneGlobalOptionSelector` occurs at
RVA `0x1d73cbe`; registration `0x1b2784e` specifies runtime size `0x1d0`.
Its original vtable is `0x27746c8`. The adapter checks the original vtable and
callback prefixes before changing only slots 25 and 33. It also checks the
unchanged metadata getter and base setter. Every other vtable slot stays original.

| Retail RVA | Contract used |
| --- | --- |
| `0x127f4c0` | Selector metadata getter |
| `0x127f500` | Three-argument event callback, slot25 |
| `0x125d560` | Original base event handling |
| `0x127f790` | Original load; fills display list, initializes default index |
| `0x127f8d0` | Original unload; player storage only when player-specific |
| `0x127f5d0` | Four-argument index callback, slot33 |
| `0x125d880` | Original index setter; bounds check and actual index readback |
| `0x97f7a0` | Original cached GUI event helper used by that setter |

The retail callbacks directly read option enum `self+0x1c8`, player-specific byte
`+0x1cc`, selected index `+0x1a0`, display string list `+0x1a8`, source string list
`+0x1c0`. The load callback reads default index `+0x190`. These offsets are checked
by executing the retail methods with explicitly synthetic objects. The matching
serialized fields are independently confirmed in original IGZ metadata by the
asset tool; no general-purpose serialized-to-runtime layout rule is assumed.
Actual game construction and localized asset loading remain original.

New assets must use the shared `menu_protocol.hpp` IDs, matching list lengths,
and player-specific=false. High project IDs bypass the retail callback's
player-options7/9 branches while retaining its base selector behavior. Unowned
IDs preserve original arguments and callbacks and never touch the PC store.

On load the adapter calls the original event handler first, then quietly seeds
the requested index through the original base setter. Event metadata is queried
through its original virtual getter; target ancestry flags and parent links match
retail traversal, with additional cycle and length guards. Unload/other events do
not seed values. No object, list or event pointer survives the synchronous call.

On a changed index, the original callback runs first. Persistence uses its actual
accepted index and the shared atomic `Store`; unsupported or out-of-domain values
are rejected. Save failure restores the previous index through the original setter
and calls the required error sink. Corrupt configuration is preserved. The startup
applied snapshot is copied once, optionally supplied explicitly by the runtime's
bootstrap, and never changed by requested saves. All edits require restart; a
saved notification does not assert renderer application.

`Hooks::available` must positively acknowledge optional FSR3/DLSS availability
using actual runtime capability information. With no provider both are rejected.
`Hooks::error` is mandatory and must visibly report failures in the integration.
Hooks run on the original calling thread, must not throw, and must not retain
guest pointers. Callbacks are serialized with a recursive mutex for reentry;
no new GUI thread or background guest writes are introduced.

Create one persistent `Adapter(image, sha, path, hooks, &startup_snapshot)`.
Call `install()` before guest threads start while original vtable memory is
writable. Keep the adapter and image alive until callbacks have stopped. Only
then call `uninstall()` while the vtable is writable. It refuses to overwrite
foreign slot changes. Destruction while installed and invalid guest contracts
fail loudly. Hot installation, hot unloading and image relocation are unsupported.

Build `bridge.cpp` and assemble `transport.asm` with MSVC/ml64. The transport
converts original SysV integer arguments to Win64 and back, preserving Win64
RDI/RSI and XMM6..15 across original calls. C++ exceptions never cross the guest
boundary. Use the latest pinned runtime JSON implementation with shared settings.

Run `powershell -NoProfile -ExecutionPolicy Bypass -File tools/settings/bridge/check.ps1`.
It extracts original bytes privately from the user's SHA-pinned dump, compiles
against latest v0.5.9 JSON, and executes original event/base-event/load/unload/index
functions. Selector/list/event/meta objects are synthetic. Two explicit synthetic
UI dependency oracles stand in for full control initialization (`0x125da70`,
default-index copy only) and generic GUI event processing (`0x125bda0`, return).
The seeding setter and its cached-event branch execute unchanged retail bytes.
Every other unresolved code page is NOACCESS. A separate synthetic event oracle
checks forwarding/return, and SIMD-clobber transport controls check ABI survival.
Invalid guest objects are checked in separate processes, which must exit4.

These controls prove the stated CPU branches and adapter contracts. They do not
prove original constructors, live localized asset loading, focus/back navigation,
or pixel presentation. The user owns visual inspection; no game automation or
screen capture is performed by this check. Neither running these original methods
nor this custom adapter increases the reconstruction count.
