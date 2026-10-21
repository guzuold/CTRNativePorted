# Public matchmaking: no dead local queue, election rounds in the log

2026-10-21. Branch `hub-diagnostics`, based on v0.5.5 (`29fb06f`). Evidence is in `hub-diagnostics-evidence/`.

## The v0.5.5 two-PC test

- PC A can be reached and elects itself hub as its service starts.
- PC B is behind the operator's NAT on a phone hotspot. Its service started 39 seconds before A's.
- B's game searched about a minute after A's service started. B had found no hub yet: it waited one second, handled the search locally, and its game waited in that queue for about two minutes, until it stopped.
- A's log has no request from B at all, so B never obtained A's address from the DHT.
- A itself found its own announcement on the DHT only three minutes after starting, 24 seconds after B stopped.

The announcement and search code is the same as in v0.5.4; the DHT's delay varies (75 seconds in the v0.5.4 test, over three minutes here).

Two weaknesses made this hard to see and impossible to recover from:
- B's local queue could never pair it with anyone, because no other service can reach B.
- No line said what each election round found, so the cause had to be reconstructed from the absence of events.

## Changes

1. **Refused search (`Server::Impl::hub_task`, `server.cpp`).**
   - On a PC that cannot be reached, a public search (145/18) that would be handled locally is refused with `UNSERVED_ERROR` and logged as `hub_none` with reason `search_refused`.
   - This covers no hub found, a sequence whose advertise was local (the hub refuses searches from players it never registered), and an unreachable hub.
   - Advertise (145/2) and token (145/3) stay local and harmless. The next advertise starts a new sequence and uses the hub if one is known.
   - A PC that can be reached keeps handling public matchmaking itself, as before.
2. **Round log (`HubElection::round_fields`, `Host::elect_hubs`).**
   - `hub_round` records `dht_results`, `verified` (identities that answered, saved or new), `candidates` (live), `reachable` and `hub` (abbreviated code or null).
   - Rounds run every 5 seconds without a hub and every 60 seconds with one.

## How the game takes the refusal

The service's real logs of 19 October 2026 contain a session where 145/2, 145/3 and 145/18 all answered with errors. The game did not stop: 60 seconds after the search it sent 145/8 and continued with session tasks.

What the player sees on screen, and whether the game searches again by itself, is not known. It must be checked in a real session with an unreachable PC and no hub.

## Tests

| Test | Before (v0.5.5) | After |
|---|---|---|
| `test_hub_wait.py`, unreachable PC without hub searches | answered locally (dead queue) | refused, `hub_none` `search_refused` |
| `test_hub_wait.py --late`, hub found after the local advertise | answered locally | refused |
| `hub_election_test.exe`, round fields | not available | DHT results, answers, candidates, hub |

- The forwarding cases (`appears`, `assigned`) still pass.
- Mixed runs with the v0.5.4 member and hub pass in both directions.
- All 18 server and HLE modes pass: 515 checks, 0 failures.

## Still to check by hand

- **Refused search on screen:** an unreachable PC without a hub. What does the game show, and does a new search after the hub appears get forwarded (`hub_task_forwarded`)?
- **`hub_round` lines:** on that PC, does `dht_results` stay at 0 until A's announcement becomes visible?
