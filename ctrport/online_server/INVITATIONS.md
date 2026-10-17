# Complete invitations and the launcher lobby (2026-10-17)

The launcher's sliding right-hand **LOBBY** drawer displays the selected room's
authenticated participant roster, not the saved address book. Open the drawer,
enable Online with **ATTIVA ONLINE** if needed, then choose **COPIA INVITO** on
the host. Guests paste the complete text into **INVITO STANZA** and choose
**ENTRA**. All controls, progress and errors are inline in the styled drawer;
there is no separate native lobby dialog. No reciprocal friend entries
or directory lookup are needed. Both launchers must include this update.

Once everyone is in the same room, start CTR and select **Online > Matchmaking >
Go to Lobby**. Existing game routing, discovery, relay, retail authentication and
room selectors remain in use. This change does not implement NAT punchthrough.
The host must be reachable exactly as for the existing direct-IP connection.
The invitation uses the address already selected by the local host service:
UPnP public address when available, otherwise LAN address. A LAN invitation is
only useful to peers that can reach that network; an invitation does not open
ports or bypass CGNAT. HTTP uses the existing fixed TCP port 8780.

Format: `CTRROOM1|host-identity|admission-token|IPv4`. The protocol version implies
the existing port set. The endpoint is verified against the included identity
before disclosing the token. Signed requests, response nonces and replay checks
remain active. The 256-bit random admission token is distinct from the retail
room selector. It lasts 15 minutes, admits up to seven guests plus the host, and
can be revoked by generating another invitation. Existing members stay admitted.
Restarting the host or changing its room invalidates its invitations. Treat the
whole text as an invitation capability: anyone holding it can request entry.
The existing signed HTTP transport is not encrypted; signatures do not provide
confidentiality against network observers.

The launcher refreshes the current host every three seconds, with one heartbeat
request in flight. Room actions are asynchronous, with controls disabled while
pending; old polling results are checked against the selected owner/room and an
operation revision before they can update the UI. Host and local-player labels
come from authenticated identities. Failures retain the last known roster and
display an error rather than claiming a successful refresh. Explicit guest leave
is reflected on the next poll; silent guests retain the existing 180-second
membership timeout. **ESCI** returns a guest to local hosting.
Changing rooms while the game is running remains disallowed.

The host is listed first, then the local guest, then other players by name.
Scroll over the roster to view the remaining participants in larger rooms.
Enter in the invitation field joins the room rather than launching the game;
Escape closes the drawer. Copy/entry/exit feedback stays inside the panel.

No automatic friend-host searches run in the lobby refresh path. The saved address
book and legacy backend APIs are preserved, but no longer drive the displayed
roster. Host migration is pending: room state replication/election and retail race
authority transfer require separate implementation and verification.

Build with `ctrport/build_launcher.ps1 -OutputDirectory build/CTRPort-room` and
`ctrport/online_server/build_online_server.ps1 -OutputDirectory build/CTRPort-room`.
Run `ctr_online_server_test.exe --room-invite-only` for invitation admission,
expiry, rotation, capacity, roster lifecycle and signed HTTP host/guest checks.
These tests are not evidence of a two-PC retail race.

## Legacy friend-invitation API (retained for compatibility)

The following describes the earlier UI/API, not the current launcher workflow.

The existing server's authenticated HTTP service (TCP 8780) stores invitations.
The launcher does not contact the invited player's IP when sending an invitation.
The recipient polls the host with a signed `invitations` request, imports the
verified result into its local inbox, and accepts through the existing `join`
request. UDP 3082, direct/relay selection, game packets and room selectors are
unchanged. No additional server, port, account or paid service is introduced.

## Use

Both players should update their launcher. Keep the lobby host running and
reachable as required for the existing online game server. Add the actual lobby
host's friend code to the recipient's address book, as well as the inviter's code
when they differ. An invitation from a guest is stored on that lobby's host, not
on the guest's computer. An unknown host cannot be discovered from the inviter's
code alone.

Select a friend and choose **Invita nella lobby**. The invitation waits on the
host for 15 minutes, including while the recipient is offline. **Inviti ricevuti**
checks known hosts and displays retrieved invitations. The main launcher also
checks one friend host every 15 seconds in round-robin order, using cached,
identity-verified addresses first and bounded DHT lookup otherwise. Larger address
books take longer to traverse. An unreachable host is not proof that no invite
exists. The explicit check has a bounded discovery budget and may need repeating
with large address books or during DHT bootstrap.

The recipient only needs outbound access to the host; its public TCP 8780 does
not need to be reachable. Its own local HTTP service remains part of the launcher
for local state. The host still needs a reachable TCP 8780 for online play and
invitations: this change does not make an unreachable host behind CGNAT reachable.

## Authentication and lifetime

Requests and responses use the existing identity signatures, fresh request nonce,
response nonce matching and replay protection. Only the authenticated recipient
can retrieve its invitation. A pull response contains the inviter and host codes,
never the room secret or roster. The receiving launcher imports only invitations
from a saved friend, verifies the host identity, and requires the returned owner
to match that host. Local inbox import requires the local identity signature.

At most 64 pending invitations are held per lobby. Repeated delivery is idempotent.
Invitations expire after 15 minutes and are consumed when joining. Changing the
host's lobby or restarting its process clears pending invitations. Inbox entries
are transient and expire as well; acceptance always rechecks the current lobby.

## Verification

`ctr_online_server_test.exe --invite-pull-only` exercises real HTTP requests with
no recipient listener at invitation creation/retrieval, recipient isolation,
secret non-disclosure, local inbox import, duplicate polling, consumption,
unchanged game room selector and expiration. `--dht-only` exercises delayed
bootstrap and diagnostics. `--dht-network-check` probes public DHT bootstrap
without publishing a player identity. These are protocol tests; they do not
constitute a two-PC retail race or a real CGNAT field test.
