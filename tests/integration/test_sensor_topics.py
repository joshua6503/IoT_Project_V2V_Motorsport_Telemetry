"""Per-variable sensor topics (v2v/{node}/sensor/{var}): dashboard parsing, bands, series, simulator."""
from __future__ import annotations

import math
import random

import pytest

import dashboard
import simulator


@pytest.mark.parametrize("topic,payload,expected", [
    ("v2v/rw612-a/sensor/brake_pct", b"42.5", ("rw612-a", "brake_pct", 42.5)),
    ("v2v/rw612-a/sensor/temp_c", b"36.8", ("rw612-a", "temp_c", 36.8)),
    ("v2v/esp32-b/sensor/distance_cm", b" 250.0\n", ("esp32-b", "distance_cm", 250.0)),
    ("v2v/rw612-a/sensor/temp_c", b"null", ("rw612-a", "temp_c", None)),
    ("v2v/rw612-a/sensor/distance_cm", b"-3.0", ("rw612-a", "distance_cm", -3.0)),
])
def test_parse_sensor_valid(topic, payload, expected):
    assert dashboard.parse_sensor(topic, payload) == expected


@pytest.mark.parametrize("topic,payload", [
    ("v2v/rw612-a/sensor/brake_pct", b"abc"),
    ("v2v/rw612-a/sensor/brake_pct", b""),
    ("v2v/rw612-a/sensor/brake_pct", b"nan"),
    ("v2v/rw612-a/sensor/brake_pct", b"inf"),
    ("v2v/rw612-a/sensor/brake_pct", b"NULL"),
    ("v2v/rw612-a/sensor/brake_pct", b"\xff\xfe"),
    ("v2v/rw612-a/sensor/tps_pct", b"1.0"),           # not a contract variable
    ("v2v/rw612-a/sensor", b"1.0"),
    ("v2v/rw612-a/sensor/brake_pct/x", b"1.0"),
    ("v2v/all/sensor/brake_pct", b"1.0"),             # reserved node id
    ("v2v/rw612-a/telemetry", b"1.0"),
    ("other/rw612-a/sensor/brake_pct", b"1.0"),
])
def test_parse_sensor_rejects(topic, payload):
    assert dashboard.parse_sensor(topic, payload) is None


def test_json_parser_ignores_sensor_topics():
    assert dashboard.parse_message("v2v/rw612-a/sensor/brake_pct", b"42.5") is None


@pytest.mark.parametrize("var,value,band", [
    ("brake_pct", 0, dashboard.GREEN), ("brake_pct", 49.9, dashboard.GREEN),
    ("brake_pct", 50, dashboard.YELLOW), ("brake_pct", 70, dashboard.RED),
    ("temp_c", 29.9, dashboard.GREEN), ("temp_c", 30, dashboard.YELLOW), ("temp_c", 35, dashboard.RED),
    ("distance_cm", 200, dashboard.GREEN), ("distance_cm", 30, dashboard.GREEN),
    ("distance_cm", 29.9, dashboard.YELLOW), ("distance_cm", 15, dashboard.YELLOW),
    ("distance_cm", 14.9, dashboard.RED),
])
def test_gauge_band(var, value, band):
    assert dashboard.gauge_band(var, value) == band


def test_gauge_text_and_null_text():
    assert dashboard.gauge_text("brake_pct", 42.54) == "42.5 %"
    assert dashboard.gauge_text("temp_c", 36.8) == "36.8 °C"
    assert dashboard.gauge_text("temp_c", None) == "--"
    assert dashboard.gauge_text("brake_pct", None) == "--"
    assert dashboard.gauge_text("distance_cm", None) == "sin eco"


def test_gauge_fraction_is_clamped():
    assert dashboard.gauge_fraction("brake_pct", -5) == 0.0
    assert dashboard.gauge_fraction("brake_pct", 50) == 0.5
    assert dashboard.gauge_fraction("distance_cm", 999) == 1.0


def test_series_store_window_and_null_gap():
    s = dashboard.SeriesStore(window_s=300)
    s.add("a", "temp_c", 20.0, now=0.0)
    s.add("a", "temp_c", None, now=100.0)
    s.add("a", "temp_c", 22.0, now=200.0)
    xs, ys = s.points("a", "temp_c", now=250.0)
    assert xs == [-250.0, -150.0, -50.0]
    assert ys[0] == 20.0 and math.isnan(ys[1]) and ys[2] == 22.0
    xs, ys = s.points("a", "temp_c", now=350.0)          # first sample is now 350 s old
    assert xs == [-250.0, -150.0] and ys[1] == 22.0
    assert s.points("a", "brake_pct", now=0.0) == ([], [])


def test_simulator_sensor_messages_per_node():
    rng = random.Random(3)
    sample = simulator.make_telemetry("rw612-a", 0, 0, rng)
    assert [leaf for leaf, _ in simulator.make_sensor_messages("rw612-a", sample)] == [
        "sensor/brake_pct", "sensor/temp_c", "sensor/distance_cm"]
    assert [leaf for leaf, _ in simulator.make_sensor_messages("esp32-b", sample)] == ["sensor/brake_pct"]


def test_simulator_sensor_text_round_trips_through_dashboard_parser():
    rng = random.Random(4)
    for seq in range(300):
        sample = simulator.make_telemetry("rw612-a", seq, 0, rng)
        for leaf, text in simulator.make_sensor_messages("rw612-a", sample):
            node, var, value = dashboard.parse_sensor(f"v2v/rw612-a/{leaf}", text.encode())
            assert (node, var) == ("rw612-a", leaf.split("/")[1])
            expected = sample[var]
            assert (value is None) if expected is None else value == pytest.approx(expected, abs=0.05)
