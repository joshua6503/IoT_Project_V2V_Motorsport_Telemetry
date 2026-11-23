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


def test_minimal_telemetry_is_valid(validator_for):
    assert validator_for("telemetry").is_valid({"node_id": "rw612-a", "ts_ms": 1, "seq": 0})


def test_prompt_example_is_valid(validator_for):
    msg = {"node_id": "rw612-a", "ts_ms": 123456, "seq": 812, "brake_pct": 42.5,
           "tps_pct": 10.0, "temp_c": 36.8, "distance_cm": 250, "sensor_ok": 7}
    validator_for("telemetry").validate(msg)


def test_nullable_sensor_fields(validator_for):
    v = validator_for("telemetry")
    assert v.is_valid({"node_id": "esp32-b", "ts_ms": 1, "seq": 1, "temp_c": None, "distance_cm": None})
    assert not v.is_valid({"node_id": "esp32-b", "ts_ms": 1, "seq": 1, "brake_pct": None})


def test_invalid_telemetry_is_rejected(validator_for):
    v = validator_for("telemetry")
    base = {"node_id": "rw612-a", "ts_ms": 1, "seq": 1}
    assert not v.is_valid({k: base[k] for k in ("node_id", "ts_ms")})       # missing seq
    assert not v.is_valid({**base, "brake_pct": 120})
    assert not v.is_valid({**base, "tps_pct": -1})
    assert not v.is_valid({**base, "sensor_ok": 256})
    assert not v.is_valid({**base, "distance_cm": -5})
    assert not v.is_valid({**base, "bogus": 1})
    assert not v.is_valid({**base, "node_id": "RW612 A"})


def test_two_node_simulator_args():
    args = simulator.parse_args([])
    assert args.node_list == ["rw612-a", "esp32-b"]
    assert simulator.parse_args(["--node-id", "sim-01"]).node_list == ["sim-01"]


def test_simulated_telemetry_sensor_ok_consistent_with_nulls(validator_for):
    rng = random.Random(3)
    for seq in range(500):
        m = simulator.make_telemetry("rw612-a", seq, seq, rng)
        if m["distance_cm"] is None:
            assert not m["sensor_ok"] & 0x01
        if m["temp_c"] is None:
            assert not m["sensor_ok"] & 0x02
