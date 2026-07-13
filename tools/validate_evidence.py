#!/usr/bin/env python3
"""Validate normalized reviewed-evidence ledgers and runtime manifests."""

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
LEDGERS = {
	"hardware_contracts": ({"id", "subsystem", "status", "confidence", "boundary", "evidence"},
		{"status": {"mapped", "prototype", "partial", "validated"},
		 "confidence": {"moderate", "strong"}}),
	"state_predicates": ({"id", "owner", "status", "predicate", "current", "evidence"},
		{"status": {"modeled", "satisfied", "unresolved", "drift"}}),
	"falsifications": ({"id", "status", "hypothesis", "conclusion", "evidence"},
		{"status": {"disproven", "unsupported"}})
}


def validate_ledger(path):
	data = json.loads(path.read_text())
	required, enums = LEDGERS.get(data.get("kind"), (None, None))
	if required is None:
		raise ValueError(f"{path}: unknown ledger kind {data.get('kind')!r}")
	if data.get("schema_version") != 1:
		raise ValueError(f"{path}: unsupported schema version")
	seen = set()
	for index, entry in enumerate(data.get("entries", [])):
		missing = required - set(entry)
		if missing:
			raise ValueError(f"{path}: entry {index} missing {sorted(missing)}")
		if entry["id"] in seen:
			raise ValueError(f"{path}: duplicate id {entry['id']}")
		seen.add(entry["id"])
		for field, allowed in enums.items():
			if entry[field] not in allowed:
				raise ValueError(f"{path}: {entry['id']} has invalid {field}={entry[field]!r}")
		for source in entry["evidence"]:
			if not (ROOT / source).is_file():
				raise ValueError(f"{path}: {entry['id']} references missing {source}")
	return len(seen)


def validate_manifests(directory):
	seen = set()
	for path in sorted(directory.glob("*.json")):
		data = json.loads(path.read_text())
		missing = {"schema_version", "id", "description", "phone", "firmware", "subsystems", "logs"} - set(data)
		if missing:
			raise ValueError(f"{path}: missing {sorted(missing)}")
		if data["schema_version"] != 1:
			raise ValueError(f"{path}: unsupported schema version")
		if data["id"] in seen:
			raise ValueError(f"{path}: duplicate id {data['id']}")
		seen.add(data["id"])
	return len(seen)


def validate_profile(path, manifest_directory):
	profile = json.loads(path.read_text())
	nodes = [item["id"] for item in profile.get("nodes", [])]
	if len(nodes) != len(set(nodes)):
		raise ValueError(f"{path}: duplicate topology node id")
	node_ids = set(nodes)
	for edge in profile.get("semantic_edges", []):
		for endpoint in ("from", "to"):
			if edge[endpoint] not in node_ids:
				raise ValueError(f"{path}: edge {edge['id']} references unknown node {edge[endpoint]}")
	manifest_data = [json.loads(item.read_text()) for item in manifest_directory.glob("*.json")]
	manifests = {item["id"]: item for item in manifest_data}
	for claim in profile.get("runtime_claims", []):
		manifest = manifests.get(claim["manifest"])
		if not manifest:
			raise ValueError(f"{path}: runtime claim {claim['id']} references unknown manifest")
		if claim["subsystem"] not in manifest["subsystems"]:
			raise ValueError(f"{path}: runtime claim {claim['id']} is outside its manifest scope")
	known_subsystems = {subsystem for item in manifest_data for subsystem in item["subsystems"]}
	for pattern in profile.get("runtime_patterns", []):
		if pattern.get("subsystem") not in known_subsystems:
			raise ValueError(f"{path}: runtime pattern {pattern['kind']} has unknown subsystem")
	return len(nodes), len(profile.get("semantic_edges", []))


def main():
	try:
		entries = sum(validate_ledger(path) for path in sorted((ROOT / "evidence").glob("*.json")))
		manifest_directory = ROOT / "tools/run_manifests"
		manifests = validate_manifests(manifest_directory)
		nodes, edges = validate_profile(ROOT / "tools/profiles/noki3210_v600.json", manifest_directory)
	except (OSError, ValueError, json.JSONDecodeError) as error:
		print(f"evidence validation failed: {error}", file=sys.stderr)
		return 1
	print(f"evidence validation: {entries} ledger entries, {manifests} runtime manifests, {nodes} nodes, {edges} edges")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
