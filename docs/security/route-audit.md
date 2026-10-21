# Route and listener audit (b95b24e, before access-policy changes)

The launcher always sets CTR_ONLINE_SERVER=127.0.0.1 (launcher.cpp, launch environment). FriendWatcher uses signed POST /ctr/v1/peer (friend_network.cpp). Party JOIN/configure/poll use signed POST /ctr/v1/party (launcher_online.cpp). Identity verification uses GET /ctr/v1/identity?nonce=... (friend_verifier.cpp), whose signed response already carries the traffic public key. Remote callers do not need /ctr/v1/traffic-key or /ctr/v1/config; the latter is fetched by local HLE discovery.

Session details, JOIN and invitation operations are forwarded inside the signed peer action webapi. peer_action restricts methods and three target prefixes and verifies the friend's player before internally calling http_answer with peer:<verified code>. Public matchmaking uses mm_task/mm_poll/mm_leave/mm_peers over the same signed channel. That internal verified context must remain permitted; a network client cannot supply it through headers.

No remote TCP lobby use was found in this architecture. TCP 3074 serves the local game; TCP 3075 and UDP 3075/3478/3479 are diagnostic recorders. Retain UDP discovery, game, relay and DHT mappings: these have actual remote roles.

Evidence is source inspection plus the existing signed peer, invitation, party and public matchmaking synthetic tests. No real two-PC captures have been established for this revision in this audit. Manual two-PC verification remains required. A manual CTR_ONLINE_SERVER pointing to a LAN/remote service is an older deployment shape and will no longer be supported: use the per-PC launcher service. Loopback aliases 127.0.0.0/8 remain supported for development.
