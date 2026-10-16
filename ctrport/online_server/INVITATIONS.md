# Invitations through the existing lobby/relay host

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
