# jech/dht (Mainline BitTorrent DHT)

This directory contains Juliusz Chroboczek's embeddable C implementation of the BitTorrent Mainline Distributed Hash Table (DHT, BEP 5).

- Upstream repository: https://github.com/jech/dht
- Author: Juliusz Chroboczek <jch@pps.jussieu.fr>
- Pinned release: `dht-0.27`
- Pinned commit: `111230894416d400c9a1e038a033586bfeaafc93`
- License: MIT (see `LICENCE`), fully compatible with GPL-2.0.

## Use in CTRNativePorted

The DHT is used exclusively for peer discovery of friend-hosted servers when friends' IP addresses change:
- Announcements are only published while the player's server is running.
- Announcements publish the contact service TCP port (8780).
- The 20-byte DHT infohash is derived with application domain separation (`CTR_DHT_DISCOVERY_v1:` || ECDSA P-256 public key).
- Local testing flag `dht_allow_loopback` allows isolated offline unit tests between local loopback instances.

## Local integration notes (review 2026-10-16)

The vendored files include Windows compatibility adaptations (Winsock send/time
callbacks and an opt-in loopback test switch); the commit above is the upstream
base, not a claim that these files are byte-identical. The MIT notice is retained.
The C implementation has process-global state: DhtService permits one live
instance per process and friend searches reuse the hosting instance. DHT node
counts are sampled on its worker thread. Tests use two processes for two nodes.
