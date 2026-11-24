"""cmd contract (DEC-013): schema, dashboard builders and simulator parser must agree."""
from __future__ import annotations

import json

import pytest

import dashboard
import simulator

VALID = [
    {"cmd": "alert", "seq": 1732371000, "origin": "dashboard", "alert_type": "manual",
     "severity": "warning", "duration_ms": 3000},
    {"cmd": "alert", "seq": 1732371001, "origin": "dashboard", "alert_type": "hard_brake",
     "severity": "critical", "duration_ms": 3000, "source_node": "rw612-a"},
    {"cmd": "alert", "seq": 0, "origin": "dashboard", "alert_type": "obstacle", "severity": "info"},
    {"cmd": "alert", "seq": 4294967295, "origin": "dashboard", "alert_type": "overtemp",
     "severity": "critical", "duration_ms": 100},
    {"cmd": "alert", "seq": 5, "origin": "dashboard", "alert_type": "manual",
     "severity": "warning", "duration_ms": 10000},
    {"cmd": "clear", "seq": 1732371002, "origin": "dashboard"},
]

BASE = {"cmd": "alert", "seq": 1, "origin": "dashboard", "alert_type": "manual", "severity": "warning"}
INVALID = [
    {**BASE, "cmd": "reboot"},
    {k: v for k, v in BASE.items() if k != "seq"},
    {k: v for k, v in BASE.items() if k != "origin"},
    {k: v for k, v in BASE.items() if k != "severity"},
    {**BASE, "duration_ms": 50},
    {**BASE, "duration_ms": 20000},
    {**BASE, "duration_ms": 1500.5},
    {**BASE, "seq": -1},
    {**BASE, "seq": 4294967296},
    {**BASE, "alert_type": "fire"},
    {**BASE, "severity": "fatal"},
    {**BASE, "origin": "Bad Origin"},
    {**BASE, "source_node": "all"},
    {**BASE, "source_node": "NOT VALID"},
    {**BASE, "bogus": 1},
    {"cmd": "clear", "seq": 1, "origin": "dashboard", "severity": "info"},
    {"cmd": "clear", "origin": "dashboard"},
    {"cmd": "actuator_test", "args": {"actuator": "buzzer"}},
]


@pytest.mark.parametrize("msg", VALID)
def test_valid_cmd_matches_schema(validator_for, msg):
    validator_for("cmd").validate(msg)


@pytest.mark.parametrize("msg", INVALID)
def test_invalid_cmd_is_rejected_by_schema(validator_for, msg):
    assert not validator_for("cmd").is_valid(msg)


def test_cmd_is_accepted_by_top_level_schema(schema):
    from jsonschema import Draft202012Validator
    Draft202012Validator(schema).validate(VALID[0])


@pytest.mark.parametrize("msg", VALID)
def test_simulator_accepts_what_schema_accepts(msg):
    parsed, reason = simulator.parse_cmd(json.dumps(msg).encode())
    assert parsed == msg and reason == ""


@pytest.mark.parametrize("msg", INVALID)
def test_simulator_rejects_what_schema_rejects(msg):
    parsed, reason = simulator.parse_cmd(json.dumps(msg).encode())
    assert parsed is None and reason


def test_simulator_rejects_bad_json_and_non_objects():
    for raw in (b"not json", b"\xff\xfe", b"[1, 2]", b'"alert"', b"42"):
        assert simulator.parse_cmd(raw)[0] is None


def test_simulator_rejects_bool_in_numeric_fields():
    assert simulator.parse_cmd(json.dumps({**BASE, "seq": True}).encode())[0] is None
    assert simulator.parse_cmd(json.dumps({**BASE, "duration_ms": True}).encode())[0] is None


def test_simulator_dedupes_by_origin_and_seq():
    seen: set = set()
    raw = json.dumps(VALID[0]).encode()
    assert "would actuate" in simulator.handle_cmd("sim-01", raw, seen)
    assert "duplicate" in simulator.handle_cmd("sim-01", raw, seen)
    other_origin = json.dumps({**VALID[0], "origin": "other"}).encode()
    assert "would actuate" in simulator.handle_cmd("sim-01", other_origin, seen)


def test_simulator_logs_clear_relay_and_ignored():
    seen: set = set()
    assert "would stop actuator" in simulator.handle_cmd("sim-01", json.dumps(VALID[5]).encode(), seen)
    line = simulator.handle_cmd("sim-01", json.dumps(VALID[1]).encode(), seen)
    assert "relay of rw612-a" in line and "hard_brake critical 3000 ms" in line
    assert "ignored" in simulator.handle_cmd("sim-01", b"garbage", seen)


def test_dashboard_builders_match_schema(validator_for):
    v = validator_for("cmd")
    v.validate(dashboard.build_alert_cmd(7, "manual", "warning"))
    v.validate(dashboard.build_alert_cmd(8, "hard_brake", "critical", 3000, source_node="esp32-b"))
    v.validate(dashboard.build_clear_cmd(9))
    # uint32 wrap keeps the seq valid
    v.validate(dashboard.build_alert_cmd(2 ** 32 + 3, "manual", "critical", 100))


def test_dashboard_builder_rejects_out_of_range_duration():
    for bad in (99, 10001, -5):
        with pytest.raises(ValueError):
            dashboard.build_alert_cmd(1, "manual", "warning", bad)


def test_dashboard_default_duration_is_3000():
    assert dashboard.build_alert_cmd(1, "manual", "warning")["duration_ms"] == 3000


def test_cmd_topics():
    assert dashboard.cmd_topic("todos") == "v2v/all/cmd"
    assert dashboard.cmd_topic("all") == "v2v/all/cmd"
    assert dashboard.cmd_topic("rw612-a") == "v2v/rw612-a/cmd"


def test_relay_targets_exclude_source_and_all():
    assert dashboard.relay_targets("rw612-a", {"rw612-a", "esp32-b", "sim-01"}) == ["esp32-b", "sim-01"]
    assert dashboard.relay_targets("rw612-a", ["rw612-a"]) == []
    assert dashboard.relay_targets("x", {"x", "all", "y"}) == ["y"]


def test_dashboard_parse_message():
    assert dashboard.parse_message("v2v/rw612-a/status", b'{"state": "online"}') == \
        ("status", "rw612-a", {"state": "online"})
    assert dashboard.parse_message("v2v/all/cmd", b"{}") is None
    assert dashboard.parse_message("v2v/rw612-a/cmd", b"{}") is None
    assert dashboard.parse_message("v2v/rw612-a/alert", b"nope") is None
    assert dashboard.parse_message("v2v/rw612-a/alert", b"[1]") is None


def test_dashboard_value_formatting():
    assert dashboard.fmt_value({"distance_cm": None}, "distance_cm", "cm", "sin eco") == "sin eco"
    assert dashboard.fmt_value({"temp_c": 36.84}, "temp_c", "°C") == "36.8 °C"
    assert dashboard.fmt_value({}, "temp_c", "°C") is None


def test_dashboard_default_args():
    args = dashboard.parse_args([])
    assert args.node_list == ["rw612-a", "esp32-b"]
    assert args.username == "dashboard"
    assert dashboard.parse_broker(args.broker) == ("192.168.0.239", 1883)
