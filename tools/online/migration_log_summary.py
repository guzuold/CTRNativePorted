# SPDX-License-Identifier: GPL-2.0-only
"""Summarize a host-loss experiment without copying tickets or packet payloads.

Pass the project's online_server_*.jsonl and optionally --after UNIX_SECONDS.
This reports backend observations only; it cannot prove continued peer gameplay.
"""
import argparse
import collections
import json
from pathlib import Path


def summarize(path, after=0):
    result = {"source": str(path), "after": after, "events": [], "tasks": {},
              "limitations": ["Backend logs do not prove peer synchronization or race continuity."]}
    counts = collections.Counter()
    documents = {}
    malformed = 0
    with Path(path).open(encoding="utf-8") as stream:
        for line in stream:
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                malformed += 1
                continue
            if row.get("time", 0) < after:
                continue
            event = row.get("event")
            if event in {"room_host_notified", "room_guest_notified", "lobby_closed", "lobby_error",
                         "EXPERIMENT_ONLY_document_promotion"}:
                result["events"].append({k: row[k] for k in
                    ("time", "event", "lobby", "host", "members", "account", "peer", "error") if k in row})
            if event != "lobby_task":
                continue
            key = f"{row.get('online_id')}: {row.get('service')}/{row.get('task')}"
            counts[key] += 1
            if row.get("service") == 145 and row.get("task") == 15:
                try:
                    document = json.loads(row["params"][2][1])
                    identity = (row.get("online_id"), document.get("update_id"),
                                document.get("listen_server", {}).get("host_player_id"))
                    if identity not in documents:
                        documents[identity] = {"first": row["time"], "last": row["time"], "count": 0,
                            "player": identity[0], "version": identity[1], "advertised_host": identity[2]}
                    documents[identity]["count"] += 1
                    documents[identity]["last"] = row["time"]
                except (KeyError, IndexError, TypeError, json.JSONDecodeError):
                    malformed += 1
    result["tasks"] = dict(sorted(counts.items()))
    result["host_document_requests"] = list(documents.values())
    result["malformed_records"] = malformed
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--after", type=float, default=0)
    args = parser.parse_args()
    print(json.dumps(summarize(args.log, args.after), indent=2))
