# CTRPort

Scene upscaling is selectable in the launcher's Settings: Off, AMD FSR 1,
DLSS (experimental, RTX required). Internal 720p–4K resolution remains independent
of the host window. See [GPU integration and verification](gpu/UPSCALING.md) for
the pre-HUD render path, NVIDIA bridge build, and known object-motion limitations.

SPDX-License-Identifier: GPL-2.0-only

R0 uses installed MSVC x64 and MASM, with no new system tools. This keeps the
reconstructed MSVC C++ objects directly linkable. Original retail code uses SysV;
explicit assembly bridges are required at each boundary. The initial synthetic
two-integer control checks Win64 → SysV → an MSVC C++ function → Win64. It does
not establish floating-point, aggregate, variadic, TLS, thread or exception ABI
support; those need separate concrete controls before retail calls use them.

Build `ctrport/build.ps1` under the infra build lock. Output is
`build/CTRPort-infra/ctrport.exe`; `--abi-control` runs the linkage control.
Other commands load pinned retail images; invocation without a supported command
fails explicitly.

Players start the game with the native launcher "Crash Team Racing Native Ported"
(`ctrport/launcher.cpp`, built by `ctrport/build_launcher.ps1` as
`build/CTRPort-dev/launcher.exe`; Win32 and GDI+ only, no Python). It
chooses the base dump and the optional v01.21 update, display resolution, fullscreen,
internal 3D resolution (720p, 900p, 1080p, 1440p, 1800p or 2160p),
60 FPS and the test unlock switch, keeps them in
`runtime/user/launcher_config.json`, and starts `ctrport.exe` with the same
command line, environment and logs as `ctrport/play_gpu.py`, which remains the
development entry point. Its artwork (`sce_sys/pic1.png`, `sce_sys/icon0.png`) is
read at run time from the dump the user selects; none is stored in the repository.

The internal resolution applies at the next game launch and is independent of
window size/fullscreen. Existing Pro/native-4K settings migrate to 1440p/2160p;
standard mode migrates to 1080p. CLI example:
`python -B ctrport/play_gpu.py --1.21 --internal-res 720 --res 1920 1080`.
See `GUEST_PATCHES.md` for the SHA-bound scale patch and render-target verification.

Design constraints for R1/R2: resolve `/app0` from the user patch folder first,
then the base folder, both read-only; writable data stays in `runtime/user`.
Reject path traversal and writes to `/app0`. Every replacement and byte patch
is keyed by the selected eboot SHA256, original address and verified report;
selection defaults to original and unmatched hashes cannot enable a replacement.
Core candidates remain disabled until retail ABI/domain checks pass in runtime.

No external code files have been copied at R0. The MASM probe and application
are new project code; no prof-derived names/types or game bytes are included.
Next reuse review: the infra oracle loader and pinned shadPS4 v0.18.0 / bbport
4fcdf770e078911cc7f36e409d316ec423b7c81e, recording license and revision per file.
R1 requires complete import inventories for both retail versions; R2 requires
observed main execution. Neither is reached by this build control.

The loader now retains eboot and six original modules, applies internal
relocations and binds exact NID/library/module/type exports to original mapped
code. `--link-images EBOOT MODULE...` reports each slot. The consolidated inventory
is (development notes): 2441 slots for
1.00 and 2448 for 1.21, with 1318 and 1322 original bindings respectively.
All remaining imports are missing; no Windows system service is implemented.
Known names come from the pinned public shadPS4 NID table. Unknown callers remain
explicitly unknown; thunk addresses do not count as callers.

`--entry-libc-boundary-images EBOOT MODULE...` executes the original 1.00 startup
and libc `_init_env`, then stops in original `atexit` at the missing libkernel
object NID `f7uOxY9mM1U`. The caller is libc SHA
`1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816`,
offset `0xad854`. The missing import exits with code 4, without returning success.
Only the reviewed libc startup page has execution permission. TLS, threads,
heap/bootstrap and writable guest file services remain pending. Main has not
been reached; zero reconstructed replacements are active. shared original-call interface is available for integer/pointer arguments
0..2 and an integer return. `OriginalCallDeclaration` binds eboot SHA, image SHA
and address; `call_original_integer2` uses the same header as the bank.
Floating argument/return passing, aggregates, variadic calls, exception unwinding
and guest TLS remain unsupported. XMM6..15 preservation is verified separately.

The harness `original_callees` descriptor and `OriginalCallSession` load genuine
retail images, bind original exports and fail missing imports explicitly.
See tools/harness/README.md for the manifest contract. Link ctrport/abi_probe.asm.
The resolver rejects incorrect SHA, non-code addresses and execution permissions;
the session enables only declared executable extents (Windows page granularity).

A complete bank composition is verified in
(development notes):
168 whole-function cases, zero differences and three rejected inputs.
The existing core metadata_finalize_layout source is compiled unchanged; its
metadata_identifier_lookup link is an infra shim calling original retail
0x13e4780, length112, rather than the reconstructed lookup source.
The independently executed original caller supplies the reference output.
Fixture rebasing is adapted from the core probe; its arrays and unsynchronized
registry are prepared synthetic inputs. Field virtual calls, live heap/manager
initialization and synchronization remain outside this verified domain.

This is harness composition evidence. Runtime replacements remain disabled;
main is not reached. Candidate owners still need to migrate their missing branches,
verify relevant domains, and provide runtime ABI/domain fallback checks. extends the shared session with `call_registers(id, arguments)` and
`OriginalRegisterArguments` / `OriginalRegisterResult`. Declare `abi: "registers"`,
`integer_count` (0..6), `xmm_types` (up to eight `float32` or `float64` entries),
and `return_type` (`integer`, `float32`, `float64`) per original callee.
The manifest is CTR_CALLS_2; integer2/CTR_CALLS_1 remain compatible.
Arguments use separate SysV integer and XMM sequences; float payloads use the
low 32 bits, double payloads all 64 bits. Results expose RAX and XMM0 bits.
Counts and types must match the declaration. Stack arguments, aggregates,
variadic calls, unwinding and guest TLS remain unsupported.
`--original-register-control` verifies synthetic routing and preservation.
`--typed-session-control` calls whole original libc fabsf; the integer3 control
calls whole original memcpy. These use synthetic inputs and enable no runtime
replacements. Owner core/gameplay bank compositions remain required for.

### Windows threads and console (2026-10-09)

`python -B ctrport/run_logged.py <runtime arguments>` copies both streams to
console and `runtime/user/logs/startup.stdout.log` / `startup.stderr.log`.
Kernel write fd1/2 and debug text also append game.stdout.log / game.stderr.log.
Other file descriptors and writes/text above1MiB fail explicitly. The console
implementation and launcher are independently written GPL-2.0-only, no copied
upstream file. Contracts: shadPS4 `src/core/libraries/kernel/debug.cpp` (debug
text -> stdout write), `file_system.cpp` (write byte count), revision
`e3ce810f3a653f43ac64ebab63023de281a4103a`, GPL-2.0-or-later.

Thread mapping is our own policy, authorized. Public attribute domain:
shadPS4 same revision `threads/pthread_attr.cpp`, FIFO/RR256..767. Our startup
supports policy1, explicit inherit0; priorities256..358 map HIGHEST,359..460
ABOVE_NORMAL,461..563 NORMAL,564..665 BELOW_NORMAL,666..767 LOWEST. The actual
478 maps NORMAL. SetThreadPriority and GetThreadPriority run before guest start.
No real-time Windows privilege is used; no PS4 timing/order equivalence is claimed.
Existing own guest_worker.hpp/startup_worker.hpp/thread_create_domain.hpp now
connect that policy to CreateThread. Owned affinity calls SetThreadAffinityMask
only for valid process-mask bits and a single processor group; unsupported hosts
fail explicitly. Original startup now reaches blocking semaphore wait, still
explicitly missing; main/R2 remains false. No engine replacement installed.

Semaphores: own implementation in kernel_semaphore.hpp from public
OrbisSem Wait/Signal/AddWaiter contract, same shadPS4 revision above,
threads/semaphore.cpp, GPL-2.0-or-later. Host condition_variable predicate waits
consume atomic token grants. Attribute1 uses FIFO,0/2 stable ascending guest
priority stored per owned worker; signal skips unsatisfied multi-token requests.
Finite timeout updates remaining microseconds without unsigned underflow.
Delete/cancel imports remain missing. Tests: --semaphore-blocking-control,
--semaphore-atomic-control, --semaphore-contention-control. Retail worker now
blocks on semaphore3; parent later terminates0xffffffff for an unresolved reason.
No main milestone is inferred from that run.

Initial-stack fix (2026-10-09): concrete retail AV at0x1ad989a writes into an
unprobed33KiB frame below committed host stack. Original startup now uses an owned
GuestStack1MiB plus128KiB HLE allowance with4KiB guard, published TEB bounds and
unchanged startup argument convention; startup is nonreturning. Independently
written abi_probe.asm/entry_boundary.hpp, GPL-2.0-only. Replay reaches missing
sceSaveDataInitialize3 rather than AV. Evidence startup_exit_debugger.json.
Reproduce diagnostic: under build-infra run powershell -File ctrport/build_trace.ps1;
trace_exit.exe <absolute ctrport.exe> <runtime arguments>. Debugger observes
exceptions and NtTerminateProcess without altering guest results;25s deadline is
explicitly reported before killing only its owned child. Debug output remains
runtime/user/logs/debug-startup.log. No system tool installation required.

SaveDataInitialize3 (2026-10-09): independently written GPL-2.0-only
save_data_initialize.hpp. Behavioral reference: shadPS4 savedata.cpp318-325 and
1206-1210, save_backup.cpp177-205, revision e3ce810f3a653f43ac64ebab63023de281a4103a,
GPL-2.0-or-later. Binds only pinned1.00/null arguments observed at startup.
Retail SFO SHA/title/firmware recorded in save_data_initialize_bringup.json;
INSTALL_DIR_SAVEDATA is absent, so title fallback CUSA14876 is used. Creates local
owned context and confined runtime/user/savedata/CUSA14876 root; maintenance
thread actually starts and waits with orderly control teardown. Mount and backup
request execution remain missing imports; no support for saving is claimed from
initialization alone. --save-data-initialize-control verifies readiness/repeat,
nonnull arguments, root escape and unknown retail context rejection. Original
startup returns successfully then reaches GetLoginUserIdList; R2 remains false.

User list (2026-10-09): own GPL-2.0-only user_service.hpp implementation; public
contract shadPS4 userservice.cpp588-610/userservice.h35/userservice_error.h at
revision e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later. Four int32
slots reflect the runtime-owned offline session created by Initialize, with
unused slots-1. Session IDs change after terminate/reinitialize; no PSN identity
or external account/config imported. --user-login-list-control verifies layout,
canaries, lifetime, null/readonly buffers and uninitialized state. Original
replay passes and reaches missing _sigprocmask. Its upstream success-only stub
is not reused. Report user_login_list_bringup.json; R2/main remains false.

Signal-mask query (2026-10-09): own GPL-2.0-only signal_mask.hpp, null-set only.
Whole pinned libc caller0xb6860/77 calls how1/setNULL/old=context+0x48; it saves
original register/FP context afterward. Snapshot reads actual thread-local owned
mask state (initially unblocked), writes exactly16bytes. Public ABI inference:
https://raw.githubusercontent.com/freebsd/freebsd-src/releng/9.0/sys/sys/_sigset.h
specifies4uint32 words; no source code from that file copied. Nonnull set/delivery
remain unsupported with NID/library/caller failure. Upstream shadPS4 kernel.cpp208
success-only stub not reused. --signal-mask-query-control tests canaries, readonly,
mutation rejection, synthetic nonzero snapshot and cross-thread isolation. Actual
original replay passes this caller and reaches missing pthread_getthreadid_np.
Reports sigprocmask_original_caller.json/signal_mask_query_bringup.json.

Numeric thread identity (2026-10-09): own GPL-2.0-only thread_identity.hpp service
pthread_getthreadid_np. Reference shadPS4 pthread.cpp314 and379-381, revision
e3ce810f3a653f43ac64ebab63023de281a4103a GPL-2.0-or-later. Uses monotonic positive
int32 IDs with actual current Windows-thread ownership; overflow and ownership
mismatch fail explicitly. --thread-numeric-id-control verifies5 distinct/stable
thread IDs and worker ownership release. Original startup reaches missing
sceKernelCreateEventFlag. Report thread_numeric_id_bringup.json. R2 remains false.

Event flag creation: independent GPL-2.0-only owned handle/state implementation; public contract shadPS4 threads/event_flag.cpp249-294 at e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later). `--event-flag-create-control` verifies state and rejection paths. Wait/set/delete remain unbound. Retail evidence: (development notes); main not reached.

Attribute affinity: independent storage from public shadPS4 pthread_attr.cpp283-319, revision e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later). Own direct guest-bit/host-bit mapping at worker creation rejects unavailable CPU masks through existing Windows affinity implementation. Controls attr-set-affinity, thread-create-domain, guest-worker-host-affinity pass. Retail mask63 setter passes; configured original worker not reached yet. No PS4 scheduling equivalence. Evidence attr_set_affinity_bringup.json.

Detach attributes: independent state0/1 storage from public shadPS4 pthread_attr.cpp197-206 at e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later). `--attr-set-detach-control` verifies readbacks and rejected inputs. Detached worker creation remains explicitly rejected. Original joinable setter0 passes; new worker entry requires review. Evidence attr_set_detach_bringup.json.

Second original worker: pinned retail entry014bb670, whole70-byte FDE verified in second_worker_entry_audit.json; caller014bb410 sets target in r12 before pthread_create. Original execution uses owned Windows thread/stack/TLS and applies configured affinity before publication. second_worker_bringup.json observes identity6 and next missing sceKernelWaitEventFlag. No reconstruction or main milestone claimed.

FIFO event wait: independent implementation of public shadPS4 event_flag.cpp105-185 and Wait API at e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later). `--event-flag-wait-control` exercises AND/OR, clear, timeout and actual parked host waiter awakened by owned internal set. Priority queue explicitly rejected; set/delete/cancel services remain unbound. Retail11 workers park on flag1, next missing SetEventFlag. Evidence event_flag_wait_bringup.json.

Event set: independent public shadPS4 event_flag.cpp Set/sceKernelSetEventFlag contract, pinned e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later). `--event-flag-set-control` verifies FIFO same-bit consumption and distinct-bit concurrent wakeup, zero-no-wake, snapshot-before-clear and invalid handles. Original mask4095 wakes workers; next missing pthread_yield. Evidence event_flag_set_bringup.json.

Thread yield: void host scheduling call matching public shadPS4 pthread.cpp418-420, pinned e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later), independent implementation. `--thread-yield-control` completes4000calls on4Windows threads preserving identities; no PS4 timing/order claim. Original next missing pthread_join handle6. Evidence thread_yield_bringup.json.

Owned thread join: independent bridge to actual Windows wait/completion state from public shadPS4 pthread.cpp112-209 at e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later). `--worker-join-bridge-control` verifies blocking, result17, NULL output and rejection paths. Original joins11workers6..16 then reaches libc data import2sWzhYqFH4E. Evidence thread_join_bringup.json. Cancellation/timed join remain unbound.

Original stdout object: public shadPS4 aerolib.inl NID2sWzhYqFH4E identifies _Stdout (revision e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later). Runtime verifies pinned libc SHA, export type1/offset120c60/size456 and binds the original mapped object, retaining relocations and original libc setup. No FILE structure copied or fabricated. Failing original read4a783 now passes; next missing pthread_cond_wait. Evidence original_stdout_data_bringup.json.

Owned condition wait: independent infinite CV wait based on public shadPS4 condvar.cpp199 and underlying wait contract at e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later). `--condition-wait-owned-control` exercises SysV bridge, mutex release to another thread, broadcast, reacquisition and recursive depth restoration. Cancellation/timed waits remain unbound. Original progresses to sceKernelOpen. Evidence condition_wait_owned_bringup.json.

Read-only open: independent GPL-2.0-only implementation of public shadPS4 file_system.cpp open/sceKernelOpen contract, revision e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later). `--kernel-open-control` verifies actual patch/base file bytes and failure paths. Only flags0 and /app0 regular files supported; unsupported operations fail explicitly. Original1.00 opens memoryconfiguration.igz; next missing sceKernelFstat. Patch precedence verified with synthetic files; original replay base-only. Evidence kernel_open_bringup.json.

Owned regular-file stat: independent implementation against public shadPS4 file_system.h:22/file_system.cpp:790, revision e3ce810f3a653f43ac64ebab63023de281a4103a GPL-2.0-or-later. Size and four timestamps come from actual Windows handle metadata; mode,512B logical block policy and zero identity fields follow the public virtual filesystem contract. `--kernel-fstat-control` verifies size513,blocks2,timestamp roundtrip,guard bytes and errors. Original1.00 size4548 progresses to sceKernelPread; no device/directory support. Evidence kernel_fstat_bringup.json.

Owned positioned read: independent GPL-2.0-only ReOpenFile/overlapped ReadFile implementation, public ABI/error contract from shadPS4 file_system.cpp:1046 at e3ce810f3a653f43ac64ebab63023de281a4103a GPL-2.0-or-later. `--kernel-pread-control` checks actual bytes,short reads,EOF,current-position preservation,guard bytes,errors. DWORD-sized regular-file reads only; unsupported sizes fail explicitly. Original reads4548bytes then reaches sceKernelClose. Evidence kernel_pread_bringup.json.

Owned close: independent GPL-2.0-only implementation of public shadPS4 file_system.cpp:295 contract at e3ce810f3a653f43ac64ebab63023de281a4103a GPL-2.0-or-later. `--kernel-close-control` checks actual handle-count decrement,descriptor removal,double-close/read-after-close EBADF. Concurrent active operation explicitly unsupported. Original60s replay timed out before close observation; no original startup progress claimed. Evidence kernel_close_bringup.json.

Runtime-owned confirm-button configuration: `CTR_ENTER_BUTTON=1` selects Cross (default), `0` selects Circle. It implements public system parameter1000; malformed values fail before output write. `--enter-button-control` checks both values, SysV2, and five preserving rejections. PadInit initializes Windows system XInput1_4 and polls four actual ports; disconnected controllers stay disconnected. PadOpen/Read remain missing. Shader emitters link once from data ctr_video.lib, no separate gnm_init.obj; VS/PS emission does not execute a shader or produce a game frame.

Exact-window observation helper: `ctrport/build_capture_window.ps1` builds
`build/CTRPort-infra/capture_window.exe` with installed MSVC/Windows SDK.
It uses Windows Graphics Capture CreateForWindow and a free-threaded D3D11 frame
pool, copies only the selected PID's visible non-minimized window, and writes a
whole-window BGRA BMP. It never changes focus. PID0 returns60; missing eligible
window returns62; unsupported capture/format returns88. Enable optional cold
captures with `CTR_COLD_CAPTURE=1` and `CTR_COLD_CAPTURE_WGC=1`; the default DXGI
client capture remains foreground-only. Captures stay in ignored runtime/user.
Implementation is original GPL-2.0-only code using Microsoft SDK APIs, with no
third-party source copied. API references:
https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow
https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframepool.createfreethreaded
Successful capture is observation evidence, not proof of title/menu or audio.

`CTR_SHORT_FRAME_OBSERVATION=1` shortens only the host diagnostic presentation
pause to2s; unset/default remains30s. The JSON hold event records the actual
configured duration. It does not skip guest waits, packets or GPU work. Cold WGC
capture samples up to32 presentations; new observations remain under runtime/user.

For measured slow original intro batches, `CTR_COLD_TIMEOUT_SECONDS` accepts an
explicit budget from30 to1800 seconds; default stays30. A timeout is a harness
stop, never title/menu evidence. Capture names include PID and run timestamp.
Batch timings include the configured observation pause; resource preparation
timing ends before ordered GPU commands and includes owner queue wait.
