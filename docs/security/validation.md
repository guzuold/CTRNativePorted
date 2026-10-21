# Security branch validation

Base: b95b24e (v0.5.4). Branch: security/v0.5.5. Date: 2026-10-21 Europe/Rome.
Repository: build/online-recovery-src, origin guzuold/CTRNativePorted. build/fresh was not edited as the working repository. No push, tag or release. All payloads in these tests are synthetic; no game files or prof-derived material are included.

## Repeatable commands

Run from the source repository on Windows with VS2022 and the pinned dependencies prepared. Local network tests need Windows access to the machine's LAN and loopback sockets; sandbox-denied traffic is not evidence of successful filtering. No game dump is needed.

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ctrport/online_server/build_online_server.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File ctrport/gpu/build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File ctrport/build_launcher.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File ctrport/hle_tests/build_incoming_policy_test.ps1
build/security-policy/incoming_policy_test.exe
build/CTRPort-gpu/ctr_hle_online_test.exe --ingress
build/CTRPort-claude/ctr_online_server_test.exe --security-json
build/CTRPort-claude/ctr_online_server_test.exe --security-blob
build/CTRPort-claude/ctr_online_server_test.exe --security-http
build/CTRPort-claude/ctr_online_server_test.exe --security-ports
~~~

Also run ctr_online_server_test with no arguments and separately with --connection-policy-only, --friend-restart-only, --peer-only, --party-migration-only, --room-invite-only and --invite-pull-only. Run ctr_hle_online_test with no arguments, --offline, --relay, --relay-block, --relay-auto-direct and --relay-auto-fallback. These separate modes are not all invoked by the default suite.

For before/after reproduction, use an independent checkout of b95b24e, copy only the current server_test.cpp and online_services_test.cpp into it, and build its original implementation. The new tests use the existing API. On this machine the original server objects were retained in build/security-baseline/online_server_obj. build_security_probe.ps1 -Baseline relinks the current test against those objects; without -Baseline it uses the current build. It does not reconstruct or silently update baseline objects. The flood-policy unit test is supplemental and requires the new implementation.

## Final results

All three requested build scripts succeeded. Final tests: **514 named checks, zero failures**, across 11 server modes (377 checks) and 7 HLE modes (137 checks), plus the 20000-address policy flood test. Every listed mode exited 0. evidence/summary.json lists counts, individual evidence/server-*.log and hle-*.log preserve check output, and build-manifest.json records binary/source SHA256. baseline-exits.log preserves the crash exit codes, which produced no normal parser-test output.

Runtime logs and synthetic identities remain under runtime/user or the existing test suites' temporary directories; the committed evidence contains check results only.

## Changes and outcomes

1. **Game ingress:** online_config.cpp / source_allowed and IncomingPolicy, net_socket.cpp / sceNetAccept, receive_common, receive_game. UDP is received into scratch space and authorized before guest copy, relay route-state mutation or discovery source mapping. Unauthorized TCP is closed before exposing a handle. Unknown sources fail closed pending a single worker, maximum four query starts per second, 256 entries/queued addresses, positive TTL 1s / negative TTL 250ms. Existing outgoing cache is also capped at 256. before-ingress.log reports five failures; after-ingress.log reports zero, with three authority requests. Covers ordinary UDP and the game port, oversized MSG_PEEK, queue consumption, a newly authorized peer without any outbound game packet, and TCP denial/acceptance. Existing relay test delivers a packet whose reported peer is 198.51.100.7, proving the real relay source is checked; discovery/auto-relay suites cover remapping and routing. policy-flood.log covers 20000 spoofed-address candidates and bounded requests/memory.

   Limits: authorization pending packets are discarded and rely on retransmission. A saturated queue can delay a new peer; it never grants permission. The exact log-once set is capped at 4096 distinct addresses, after which new-address logs are suppressed while the aggregate counter continues. Receive processing yields EWOULDBLOCK after 64 rejected/control packets in one call. No claim of successful real NAT traversal is made.

2. **Parser recursion:** json.cpp / Parser::value and string, protocol.cpp / Reader::value. Both malicious baseline processes exited -1073741571 (0xC00000FD); corrected probes exit 0 after catching normal exceptions. The HTTP baseline also crashes; corrected HTTP returns 403 and a subsequent config request succeeds. See parser-audit.md for the other parsers, dump/destructor scope and 1 MiB stack boundary checks.

3. **HTTP identity boundary:** server.cpp / http_answer permits external POST /ctr/v1/peer, POST /ctr/v1/party and GET /ctr/v1/identity with optional query. All other external HTTP requests receive 403 before forwarding or local account computation. Internally verified peer forwarding is preserved. route-audit.md records the source review made before changing the policy, including traffic-key/config and the launcher's fixed loopback address. Baseline LAN invitations/config/key/auth3/WebAPI checks fail; corrected checks pass, while identity and signed peer/party requests from the LAN still work.

4. **Hub:** hub-options.md contains three options, costs, trust and Sybil limitations. No election, sampling or quota implementation was changed.

5. **Resource limits:** server.cpp / track, release, accept_loop, read_deadline, recv_exact, handle_http. HTTP header and body have separate absolute 5s deadlines, including drip traffic. TCP admission is atomic under the existing worker mutex: 128 global, 8 per IP; counts are released on every normal/error close and thread-start failure. Writes have a 5s idle timeout; lobby reads have a distinct 300s idle timeout. Before/after HTTP logs cover silence, incomplete body, header drip, immediate refusal at per-IP/global limits, service of admitted connections, and a lobby connection surviving beyond the HTTP timeout. The lobby test is a synthetic idle socket, not a full gameplay session or a five-minute endurance test.

6. **Ports:** launcher_online.cpp / host_ports removes TCP 3074 from new UPnP requests. server.cpp / start binds lobby and TCP/UDP diagnostic recorders only on loopback, retaining explicitly configured loopback aliases for multi-service tests. Remote discovery, game UDP, relay and DHT remain unchanged. before-ports.log has six failures (one UPnP mapping plus five wildcard bindings); after-ports.log passes. Existing manual or leased router mappings are not actively deleted, but no lobby/recorder listener binds the public/LAN interface.

## Findings in existing tests

Two separate suites already failed on b95b24e: peer-only and connection-policy-only. Their fixtures still used user_id(name), although v0.5.4 uses player_account(service_code,name), and an error-message assertion expected an obsolete Italian string. Updating these fixtures exposed an actual preexisting invitation failure: SessionStore::invite compared the service-scoped owner to user_id(sender). The correction checks the session owner and its registered member name instead. Synthetic store fixtures now register the owner, as the production create route already does; a spoofed sender-name negative check is retained. Baseline failures and the corrected-fixture comparison are included as evidence, without attributing the old defect to these security changes.

## Manual two-PC checks still required

- Mixed v0.5.4/current and current/current: friends presence, restart and reconnect. Watch peer_presence, peer_rejected, http_source_rejected and connection_error.
- Private JOIN/invitations: create, send, receive, accept, details, members and leave in both directions. Watch game_invitation_send/poll/accept, signed peer forwarding and connection permissions.
- Public matchmaking: two nonfriends assigned together, a third player in another playlist, cancellation and reassignment. Confirm only assigned addresses become authorized.
- NAT/direct and forced relay: first inbound packet before outbound traffic, relay DELIVER's reported peer, discovery mapping and direct-to-relay fallback. Watch incoming_blocked, relay_first_deliver, relay_auto_direct_confirmed and relay_auto_fallback. A pending drop must resolve on retransmission rather than persist indefinitely.
- Long menu/playing sessions and router behavior: verify lobby keepalives, no premature disconnect and no new TCP 3074 mapping. Check limits with multiple legitimate players sharing an IP; eight simultaneous service connections per IP is the intended admission ceiling.

Wire formats are unchanged. Per-PC launcher services remain protocol-compatible with v0.5.4; actual two-PC gameplay has not been verified here. Direct LAN/remote CTR_ONLINE_SERVER deployments are intentionally incompatible, documented in CHANGELOG.

Runtime milestones: build evidence only in this security task; no new title/menu/track/race claim. Reconstruction: zero new functions reconstructed or activated by this work.

## Commits

- 28d2150 — ingress filtering, bounded asynchronous authorization and socket/flood tests.
- 2e8cb52 — JSON depth bound and nonrecursive typed lengths.
- 868eefd — HTTP source boundary, connection deadlines/caps, local listeners/UPnP, invitation ownership correction and regression evidence.
- The following documentation commit adds this report, route/parser audits, hub proposal, README and Unreleased changelog. Use `git log --oneline b95b24e..security/v0.5.5` for the complete immutable list including that commit.
