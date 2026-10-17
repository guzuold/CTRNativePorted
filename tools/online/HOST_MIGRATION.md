# Host migration investigation

Status (2026-10-17): **experimental; disabled by default**. Three-client
Dingo Canyon and four-client Polar Pass retail EU1.21 tests preserved active
races after losing both the original host and backend. Polar Pass survived a
second host loss in the same race, with reciprocal movement and stable session
pointers. This is local testing, not Internet/NAT or relay failover validation.

Signed launcher-room checkpoints and successor recovery pass HTTP integration
tests with three separate local servers. The combined launcher UI recovery flow
has not been validated end to end with the retail race. The game's service
connection is not retargeted yet: returning to the lobby can report "Unable to
connect to servers". Wallet migration and relay failover remain unimplemented.

Use `CTR_EXPERIMENTAL_HOST_MIGRATION=1` on both the project server and test runtime.
`CTR_RUNTIME_EXE` selects the isolated runtime through `play_gpu.py`. The adapter is
pinned to the retail hash below and checks displaced instruction signatures. It
admits only known room members during a 30-second transition. See the dated report
for tested variants, regressions, corrected authority handling and raw evidence
locations. These are runtime adaptations, not verified reconstructed functions.

## Reproduced failure

Three isolated retail EU 1.21 clients joined the same Dragon Mines race.
Only the host runtime was terminated; the local experimental backend remained
available. It promoted a surviving participant and sent the existing document
update event. Both guests subsequently advertised that new owner, but each
marked the other guest as dropped.

The one-shot diagnostic observed this sequence in **both** surviving processes:

| Retail image offset | Descriptive observation |
| --- | --- |
| `0xa44c00` | Peer-disconnect callback; current session is non-null, role value 2. |
| `0xa44190` | Session replacement requested with a null second argument. |
| `0xa441f2` | Call site that disconnects all replication peers, before the session pointer is cleared. |

The session-replacement caller was `0xa44d39`, the return address of the call
inside the host-disconnect branch. The same manager and session pointers were
observed throughout each process's sequence. Field reads succeeded in both.
These descriptions are ours; the observations were verified on the retail
executable, not inferred solely from a reference build.

Pinned retail eboot SHA256:
`39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5`.
Offsets do not apply to other builds. This is diagnostic evidence, not a
verified C++ reconstruction or a migration patch.

## Repeating the observation

Build `tools/online/build_host_loss_trace.ps1` with the installed MSVC toolchain.
Once an isolated test guest is in a real race, run:

```text
build/CTRPort-migration-probe/runtime_host_loss_trace.exe GUEST_PID 120 RETAIL_SHA256
```

The process must be named `ctrport.exe`. The hash argument selects the supported
version; it does **not** hash the running image. Independently verify the supplied
retail executable. Two entry instruction signatures locate the mapped image.
Do not use this diagnostic on a personal session or another executable renamed
to match. It briefly pauses the target and temporarily inserts four one-shot
breakpoints. It restores each original instruction and resumes at that
instruction, without changing game semantics. A timeout also restores remaining
breakpoints and detaches. Return 0 means successful cleanup; nonzero is failure.
Output contains local memory addresses and belongs in `runtime/user`, not in a
published capture. No focus change or global keyboard input is involved.

Observe `armed` before terminating only the known test host PID. Keep the backend
and at least two guests alive. Inspect game logs as well as screenshots: a moving
kart after disconnection can be an offline continuation. One surviving client
cannot demonstrate continued multiplayer synchronization.

Validation completed: MSVC C++17 `/W4 /WX`; three-second attach/timeout/detach;
all three breakpoints hit on each of two real guests; independent process-memory
reads confirmed both entry signatures and the call-site opcode were restored;
both processes continued rendering afterwards. These checks cover the successful
and timeout paths, not every Windows debugger error condition.

Local evidence: `runtime/user/migration_trace_20261017/` (traces, restoration
checks, binary hashes, compatibility calls, backend summary and screenshots).
All test processes were closed by exact PID afterwards.

## Remaining implementation boundary

Candidate routes are cached before failure; the adapter preserves surviving
object ownership and ignores teardown of a different auxiliary session. Local
whole-host and repeated-host-loss tests now pass. Run
`ctrport/test_race_migration_routes.ps1` for synthetic parser checks and the server
test executable with `--party-migration-only` for signed HTTP room recovery.

Still required: retarget the game's authenticated backend connection, authorize
new public peer addresses after backend loss, relay continuity, Internet/NAT
tests, and combined launcher/game end-to-end recovery. Room recovery selects a
known successor; network partitions and an unreachable successor are not solved.

Runtime milestone: no completed host-migration/R8 milestone. Reconstruction:
zero new verified replacements. The tested runtime reported 21 active R8
compatibility stubs and called eight distinct registry entries; the registry was
not changed. The exact called entries are recorded in the local evidence and
the investigation report.
