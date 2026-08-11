#!/usr/bin/env python3
"""Maintain a topic evidence registry for RVV runs.

The registry is intentionally simple: official Make/script targets can call
`record` after writing logs, summaries, manifests, or doctor reports. Worker
recovery and pre-commit checks can call `check` to find evidence files that were
overwritten or newly generated without a matching documentation refresh.
"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import sys
import time
from pathlib import Path
from typing import Any

SCHEMA_VERSION = 1
DEFAULT_FAIL_ON = {"never", "changed", "unregistered", "missing-doc", "any"}


def _repo_root(start: Path) -> Path:
    current = start.resolve()
    if current.is_file():
        current = current.parent
    for path in (current, *current.parents):
        if (path / ".git").exists():
            return path
    return Path.cwd().resolve()


def _normalize(path: str | Path, root: Path) -> str:
    resolved = Path(path)
    if not resolved.is_absolute():
        resolved = (Path.cwd() / resolved).resolve()
    try:
        return resolved.relative_to(root).as_posix()
    except ValueError:
        return resolved.as_posix()


def _sha256(path: Path) -> str | None:
    if not path.exists() or not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def _file_state(path: str | Path, root: Path) -> dict[str, Any]:
    raw = Path(path)
    resolved = raw if raw.is_absolute() else (Path.cwd() / raw)
    resolved = resolved.resolve()
    exists = resolved.exists()
    state: dict[str, Any] = {
        "path": _normalize(resolved, root),
        "exists": exists,
    }
    if exists and resolved.is_file():
        stat = resolved.stat()
        state.update(
            {
                "size": stat.st_size,
                "mtime_ns": stat.st_mtime_ns,
                "sha256": _sha256(resolved),
            }
        )
    return state


def _load_registry(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {
            "schema_version": SCHEMA_VERSION,
            "updated_at_epoch": None,
            "files": {},
            "runs": [],
        }
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError(f"registry is not a JSON object: {path}")
    data.setdefault("schema_version", SCHEMA_VERSION)
    data.setdefault("files", {})
    data.setdefault("runs", [])
    return data


def _write_registry(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    data["schema_version"] = SCHEMA_VERSION
    data["updated_at_epoch"] = int(time.time())
    with path.open("w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
        handle.write("\n")


def _related_paths(args: argparse.Namespace) -> dict[str, str]:
    related: dict[str, str] = {}
    for key in ("summary", "manifest", "doctor"):
        value = getattr(args, key)
        if value:
            related[f"{key}_path"] = value
    return related


def cmd_record(args: argparse.Namespace) -> int:
    root = _repo_root(Path(args.registry))
    registry_path = Path(args.registry)
    registry = _load_registry(registry_path)
    paths = list(args.path or [])
    for value in (args.summary, args.manifest, args.doctor):
        if value:
            paths.append(value)
    paths = sorted(dict.fromkeys(paths))
    if not paths:
        print("evidence_registry: no --path/--summary/--manifest/--doctor provided", file=sys.stderr)
        return 2

    run_files: list[str] = []
    related = {k: _normalize(v, root) for k, v in _related_paths(args).items()}
    for path in paths:
        state = _file_state(path, root)
        rel_path = state["path"]
        run_files.append(rel_path)
        registry["files"][rel_path] = {
            "state": state,
            "target": args.target,
            "backend": args.backend,
            "run_label": args.run_label,
            "case_filter": args.case_filter,
            "evidence_role": args.evidence_role,
            "related": related,
            "doc_refs": args.doc_ref or [],
            "freshness_state": args.freshness_state,
            "recorded_at_epoch": int(time.time()),
        }

    registry["runs"].append(
        {
            "target": args.target,
            "backend": args.backend,
            "run_label": args.run_label,
            "case_filter": args.case_filter,
            "evidence_role": args.evidence_role,
            "files": run_files,
            "recorded_at_epoch": int(time.time()),
        }
    )
    _write_registry(registry_path, registry)
    print(f"recorded {len(paths)} evidence file(s) in {_normalize(registry_path, root)}")
    return 0


def _compare_state(recorded: dict[str, Any], current: dict[str, Any]) -> list[str]:
    if not current.get("exists"):
        return ["missing"]
    changes: list[str] = []
    for key in ("size", "mtime_ns", "sha256"):
        if recorded.get(key) != current.get(key):
            changes.append(key)
    return changes


def _iter_scan_paths(patterns: list[str], root: Path) -> set[str]:
    found: set[str] = set()
    for pattern in patterns:
        raw_pattern = Path(pattern)
        if raw_pattern.is_absolute():
            base = Path("/")
            glob_pattern = raw_pattern.as_posix().lstrip("/")
        else:
            base = root
            glob_pattern = pattern
        for path in base.glob(glob_pattern):
            if path.is_file():
                found.add(_normalize(path, root))
    return found


def _load_doc_text(paths: list[str], root: Path) -> str:
    chunks: list[str] = []
    for value in paths:
        path = Path(value)
        if not path.is_absolute():
            path = root / path
        if path.exists() and path.is_file():
            chunks.append(path.read_text(encoding="utf-8", errors="ignore"))
    return "\n".join(chunks)


def _doc_tokens(entry_path: str, entry: dict[str, Any]) -> list[str]:
    tokens = [entry_path]
    run_label = entry.get("run_label")
    if run_label:
        tokens.append(str(run_label))
    related = entry.get("related") or {}
    for value in related.values():
        if value:
            tokens.append(str(value))
    return sorted(set(tokens))


def cmd_check(args: argparse.Namespace) -> int:
    root = _repo_root(Path(args.registry))
    registry_path = Path(args.registry)
    registry = _load_registry(registry_path)
    files = registry.get("files", {})
    issues: list[tuple[str, str, str]] = []

    for rel_path, entry in sorted(files.items()):
        recorded_state = (entry or {}).get("state") or {}
        current_state = _file_state(root / rel_path, root)
        changes = _compare_state(recorded_state, current_state)
        if changes:
            issues.append(("unregistered_change", rel_path, ",".join(changes)))

    for rel_path in sorted(_iter_scan_paths(args.scan_glob or [], root)):
        if rel_path not in files and not _excluded(rel_path, args.exclude_glob or []):
            issues.append(("unregistered_file", rel_path, "scan_glob"))

    if args.require_doc_ref:
        doc_text = _load_doc_text(args.doc or [], root)
        for rel_path, entry in sorted(files.items()):
            if not doc_text:
                issues.append(("doc_ref_missing", rel_path, "no_doc_inputs"))
                continue
            tokens = _doc_tokens(rel_path, entry or {})
            if not any(token and token in doc_text for token in tokens):
                issues.append(("doc_ref_missing", rel_path, "not_referenced"))

    if not issues:
        print("evidence registry check: fresh")
        return 0

    print("evidence registry check: issues")
    for kind, rel_path, detail in issues:
        print(f"- {kind}: {rel_path} ({detail})")

    if args.fail_on == "never":
        return 0
    if args.fail_on == "any":
        return 1
    if args.fail_on == "changed" and any(kind == "unregistered_change" for kind, _, _ in issues):
        return 1
    if args.fail_on == "unregistered" and any(kind == "unregistered_file" for kind, _, _ in issues):
        return 1
    if args.fail_on == "missing-doc" and any(kind == "doc_ref_missing" for kind, _, _ in issues):
        return 1
    return 0


def _excluded(path: str, patterns: list[str]) -> bool:
    return any(fnmatch.fnmatch(path, pattern) for pattern in patterns)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    record = subparsers.add_parser("record", help="record evidence output state")
    record.add_argument("--registry", required=True, help="path to evidence_registry.json")
    record.add_argument("--path", action="append", help="evidence file to record")
    record.add_argument("--summary", help="summary artifact path")
    record.add_argument("--manifest", help="evidence_manifest.json path")
    record.add_argument("--doctor", help="evidence_doctor.md/json path")
    record.add_argument("--target", required=True, help="producer Make/script target")
    record.add_argument("--backend", required=True, choices=["qemu", "board", "target", "host", "unknown"])
    record.add_argument("--run-label", required=True)
    record.add_argument("--case-filter", default="")
    record.add_argument("--evidence-role", default="summary")
    record.add_argument("--doc-ref", action="append", help="known document reference")
    record.add_argument("--freshness-state", default="recorded")
    record.set_defaults(func=cmd_record)

    check = subparsers.add_parser("check", help="check registry freshness")
    check.add_argument("--registry", required=True, help="path to evidence_registry.json")
    check.add_argument("--scan-glob", action="append", help="glob for evidence outputs to detect")
    check.add_argument("--exclude-glob", action="append", help="ignore matching scan paths")
    check.add_argument("--doc", action="append", help="markdown doc to search for registry references")
    check.add_argument("--require-doc-ref", action="store_true")
    check.add_argument("--fail-on", default="never", choices=sorted(DEFAULT_FAIL_ON))
    check.set_defaults(func=cmd_check)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
