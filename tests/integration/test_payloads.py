"""Schema checks that need no broker: the simulator must emit contract-valid payloads."""
from __future__ import annotations

import random

from jsonschema import Draft202012Validator

import simulator


def test_schema_is_valid_draft_2020_12(schema):
    Draft202012Validator.check_schema(schema)


def test_simulated_telemetry_matches_schema(validator_for):
    v = validator_for("telemetry")
    rng = random.Random(1)
    for seq in range(200):
        v.validate(simulator.make_telemetry("sim-01", seq, seq * 200, rng))


def test_simulated_alert_matches_schema(validator_for):
    v = validator_for("alert")
    rng = random.Random(2)
    for seq in range(50):
        v.validate(simulator.make_alert("sim-01", seq, seq * 1000, rng))


def test_status_matches_schema(validator_for):
    v = validator_for("status")
    v.validate(simulator.make_status("sim-01", True))
    v.validate(simulator.make_status("sim-01", False))


def test_alert_with_unknown_type_is_rejected(validator_for):
    bad = {"node_id": "sim-01", "ts_ms": 0, "seq": 0, "alert_type": "fire", "severity": "info"}
    assert not validator_for("alert").is_valid(bad)
