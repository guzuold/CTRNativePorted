# Security follow-up: ingress renewals, local connection quota, matchmaking sequence

2026-10-21, Europe/Rome. Branch `security-followup`, based on `hub-election` (`d0de95a`). No push, tag or release. All payloads are synthetic. The user's launcher was closed for every run (each script checks `launcher.exe` and `ctrport.exe` first). Evidence is in `followup-evidence/`.

## 1. Authorized peers lost datagrams at every renewal

**Cause.** `IncomingPolicy` (`ctrport/hle/online_config.cpp`) kept a positive answer for one second. When it expired, `check()` refused the peer until the service answered again. A single worker served one address at a time and paused 250 ms after each answer, so with several opponents the renewal round lasted longer than the one-second validity.

**Change.**
- Two queues. Renewals of authorized peers go first and have no rate limit; unknown sources keep the limit of four queries per second.
- A renewal is asked one second after each positive answer.
- A peer that sent a datagram in the last second stays accepted while its renewal is in flight.
- A refusal takes effect when the service answers. If the service does not answer, the authorization ends three seconds after the last positive answer.
- A peer that went quiet for over a second waits for its renewal's answer. This preserves the existing `--ingress` check, where a peer revoked while silent must be refused 1.2 s later.

**Test.** `incoming_policy_test.exe` (the production policy with an injected service) gains steady-traffic, flood and revocation modes. `ctr_hle_online_test.exe --ingress-steady <ms>` runs the real `ctr_hle.dll`: an authorized peer sends 60 datagrams/s for 6 s.

| Case | Before | After |
|---|---|---|
| Policy, 1 peer, service 0 / 30 / 100 ms | 4 / 8 / 28 refused | 0 / 0 / 0 |
| Policy, 7 peers, service 100 ms, 5 s at 60 Hz | 1255 refused | 0 |
| Policy, 7 peers during a spoofed-source flood | 1752 refused | 0 |
| DLL, 1 peer, service 0 / 30 / 100 ms (360 sent) | 355 / 350 / 326 received | 360 / 360 / 360 |
| Revocation while sending | refused at once | refused 32–41 ms after the service answers |
| Service unreachable | refused at once | refused after about 2 s (within 3 s) |

The "before" policy runs use the same test source built against `online_config.cpp` from `hub-election`; the "before" DLL runs use the `hub-election` DLL. See `before-policy.log`, `after-policy.log`, `before-hle.log` and `after-hle.log`.

**Limits.**
- Only one LAN address was available, so the real-DLL test has one peer; the multi-peer cases use the injected policy.
- A peer that has been quiet for over a second loses its datagrams until the answer arrives. Race traffic is continuous; lobby traffic was not measured.
- Under a spoofed-source flood, a newly assigned peer may still wait behind up to 256 queued unknown addresses. That is unchanged and never grants permission.

## 2. Blocking receive failed with EWOULDBLOCK under a refused flood

**Cause.** `receive_game` (`ctrport/hle/net_socket.cpp`) stopped after 64 refused datagrams and returned EWOULDBLOCK, even for a blocking call.

**Change.** A blocking call (the socket has no `SO_NBIO` and the call has no `MSG_DONTWAIT`) keeps reading. `recvfrom` blocks between datagrams, so this costs no idle CPU. Non-blocking calls keep the 64 limit.

**Test.** `ctr_hle_online_test.exe --ingress-blocking`: 100 refused datagrams, then one from loopback, then a blocking receive. Before, it returned `0x80410123` (EWOULDBLOCK); after, it returns the 2-byte accepted datagram.

## 3. Remote connections could cut the local game off its service

**Cause.** `track()` (`ctrport/online_server/server.cpp`) counted the local game against the 128 global and 8 per-IP limits. Sixteen remote addresses holding eight connections each blocked the game's own requests, including the peer authorizations above.

**Change.**
- Connections from 127.0.0.1 have their own quota of 64 and do not count against the remote limits.
- Other loopback aliases still count as remote; the tests use them to stand in for remote addresses.
- The per-IP test now uses the alias 127.0.2.1.

**Test.** `--security-http` gains "local game served during global saturation and past eight local connections". It fails before the change and passes after (`before-connections.log`, `after-connections.log`).

## 4. Lobby idle timeout removed

The five-minute read timeout set for every accepted TCP connection is gone. HTTP keeps its own five-second deadlines, and the lobby and diagnostic recorders listen on loopback only, so the timeout defended against nothing and could disconnect a game left in a menu. The write timeout stays. No test covers a five-minute idle session.

## 5. Matchmaking sequence split between local service and hub

**Cause.** The hub records a player's advertise (145/2) and then accepts that player's search (145/18) only with the same room and token (`MatchmakingStore`, `stores.cpp`). If no hub was known for the advertise, it was handled locally. A hub elected a moment later then received 145/3 and 145/18 and refused the search.

**Change.**
- `hub_task` remembers a player whose sequence is handled locally (`local_seats`) and keeps the rest of that sequence local.
- A new advertise chooses again.
- A disconnect clears it, as it clears a hub seat.
- The same applies when this service is the hub or forwarding fails.

**Test.** `test_hub_wait.py --late`: the fixture publishes the hub 100 ms after the advertise fell back locally. Before, the search was forwarded and refused; after, it is answered locally and nothing of the sequence is forwarded (`before-sequence.log`, `after-sequence.log`). Codex's "absent" case now expects one `hub_none` and one wait, because the follow-up task stays local without waiting again.

## Regression runs (final build)

- Builds: `build_online_server.ps1`, `ctrport/gpu/build.ps1`, `ctrport/build_launcher.ps1` and `build_hub_test.ps1` succeeded.
- Server and HLE suites: all 18 modes listed in validation.md exit 0, with 515 checks and 0 failures (`suites/suite-exits.json` and per-mode logs).
- Policy tests: 20 checks, 0 failures.
- Steady and blocking modes: all pass.
- `hub_election_test.exe`: 22 checks, 0 failures.
- `test_hub_wait.py`: all checks pass, including `--late`.
- Mixed-version runs against the v0.5.4 hub and member builds in `build/hub-v054` pass in both directions.

## Still to verify by hand

- A two-PC race, then a race with more than two players, in public matchmaking. Watch `incoming_blocked` in `game.stdout.log`: it should appear only for addresses that were never assigned.
- A game left in the online menu for over five minutes stays connected.
- The CGNAT check from hub-election.md.

Runtime milestones: builds and local tests only; no new game milestone. Reconstruction: no functions reconstructed or activated.
