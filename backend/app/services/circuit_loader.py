from __future__ import annotations

import json
import os
import re
from pathlib import Path


DATA_DIR = Path(__file__).resolve().parents[3] / "data" / "sample_circuits"

_SAFE_ID = re.compile(r'^[A-Za-z0-9_-]+$')


def list_circuits() -> list[dict[str, str]]:
    circuits: list[dict[str, str]] = []
    for path in sorted(DATA_DIR.glob("*.json")):
        circuits.append(
            {"id": path.stem, "name": path.stem.replace("_", " ").title()}
        )
    return circuits


def load_circuit(circuit_id: str) -> dict:
    path = get_circuit_path(circuit_id)
    return json.loads(path.read_text(encoding="utf-8-sig"))


def get_circuit_path(circuit_id: str) -> Path:
    if not _SAFE_ID.fullmatch(circuit_id):
        raise FileNotFoundError(f"Invalid circuit id: {circuit_id!r}")
    candidate = (DATA_DIR / f"{circuit_id}.json").resolve()
    base = DATA_DIR.resolve()
    if not str(candidate).startswith(str(base) + os.sep):
        raise FileNotFoundError(f"Invalid circuit id: {circuit_id!r}")
    return candidate
