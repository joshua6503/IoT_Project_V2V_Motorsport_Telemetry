"""Shared fixtures: schema loading and simulator import."""
from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest
from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_PATH = ROOT / "interfaces" / "payload.schema.json"

sys.path.insert(0, str(ROOT / "tools" / "node-simulator"))


@pytest.fixture(scope="session")
def schema() -> dict:
    return json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))


@pytest.fixture(scope="session")
def validator_for(schema):
    """Return a validator bound to one $defs entry, e.g. validator_for('telemetry')."""
    def _make(name: str) -> Draft202012Validator:
        sub = {"$schema": schema["$schema"], "$defs": schema["$defs"], "$ref": f"#/$defs/{name}"}
        return Draft202012Validator(sub)
    return _make
