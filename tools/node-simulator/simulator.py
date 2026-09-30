#!/usr/bin/env python3
"""Fake V2V node: publishes telemetry and occasional alerts over MQTT.

Topics and payloads follow interfaces/mqtt-topics.md and
interfaces/payload.schema.json. Values are random but schema-valid.
"""
from __future__ import annotations

import argparse
import json
import random
import sys
import time

import paho.mqtt.client as mqtt

ALERT_TYPES = ["manual", "hard_brake", "obstacle", "overtemp"]
SEVERITIES = ["info", "warning", "critical"]
UINT32_MAX = 0xFFFFFFFF
UINT16_MAX = 0xFFFF


def topic(node_id: str, leaf: str) -> str:
    return f"v2v/{node_id}/{leaf}"


def make_telemetry(node_id: str, seq: int, ts_ms: int, rng: random.Random) -> dict:
    return {
        "node_id": node_id,
        "ts_ms": ts_ms & UINT32_MAX,
        "seq": seq & UINT32_MAX,
        "speed_kmh": round(rng.uniform(0.0, 90.0), 2),
        "accel_g": {
            "x": round(rng.gauss(0.0, 0.3), 3),
            "y": round(rng.gauss(0.0, 0.2), 3),
            "z": round(rng.gauss(1.0, 0.05), 3),
        },
        "temp_c": round(rng.uniform(25.0, 95.0), 1),
        "distance_cm": None if rng.random() < 0.05 else round(rng.uniform(20.0, 400.0), 1),
    }


def make_alert(node_id: str, seq: int, ts_ms: int, rng: random.Random) -> dict:
    return {
        "node_id": node_id,
        "ts_ms": ts_ms & UINT32_MAX,
        "seq": seq & UINT16_MAX,
        "alert_type": rng.choice(ALERT_TYPES),
        "severity": rng.choice(SEVERITIES),
    }


def make_status(node_id: str, online: bool) -> dict:
    return {"node_id": node_id, "state": "online" if online else "offline"}


def parse_broker(value: str) -> tuple[str, int]:
    host, _, port = value.partition(":")
    return host or "localhost", int(port) if port else 1883


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--broker", default="localhost:1883",
                   help="broker as host[:port] (default: %(default)s)")
    p.add_argument("--node-id", default="sim-01",
                   help="node id used in topics and payloads (default: %(default)s)")
    p.add_argument("--rate", type=float, default=5.0,
                   help="telemetry rate in Hz (default: %(default)s)")
    p.add_argument("--alert-prob", type=float, default=0.02,
                   help="probability of an alert per telemetry sample (default: %(default)s)")
    p.add_argument("--count", type=int, default=0,
                   help="stop after N telemetry messages, 0 = run forever (default: %(default)s)")
    p.add_argument("--seed", type=int, default=None, help="random seed for reproducible runs")
    args = p.parse_args(argv)
    if args.rate <= 0:
        p.error("--rate must be > 0")
    return args


def run(args: argparse.Namespace) -> int:
    rng = random.Random(args.seed)
    host, port = parse_broker(args.broker)
    node = args.node_id

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"{node}-simulator")
    client.will_set(topic(node, "status"), json.dumps(make_status(node, False)), qos=1, retain=True)

    def on_connect(c, _userdata, _flags, reason_code, _props):
        print(f"connected to {host}:{port} ({reason_code})")
        c.publish(topic(node, "status"), json.dumps(make_status(node, True)), qos=1, retain=True)
        c.subscribe(topic(node, "cmd"), qos=1)

    def on_message(_c, _userdata, msg):
        print(f"cmd received on {msg.topic}: {msg.payload!r} (commands are TBD, ignored)")

    client.on_connect = on_connect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=10)
    client.connect(host, port, keepalive=30)
    client.loop_start()

    period = 1.0 / args.rate
    t0 = time.monotonic()
    seq = alert_seq = 0
    try:
        while args.count == 0 or seq < args.count:
            ts_ms = int((time.monotonic() - t0) * 1000)
            tlm = make_telemetry(node, seq, ts_ms, rng)
            client.publish(topic(node, "telemetry"), json.dumps(tlm), qos=0)
            if rng.random() < args.alert_prob:
                alert = make_alert(node, alert_seq, ts_ms, rng)
                client.publish(topic(node, "alert"), json.dumps(alert), qos=1)
                print(f"alert: {alert}")
                alert_seq += 1
            seq += 1
            time.sleep(period)
    except KeyboardInterrupt:
        pass
    finally:
        # Clean disconnect does not fire the LWT, so publish offline explicitly.
        client.publish(topic(node, "status"), json.dumps(make_status(node, False)),
                       qos=1, retain=True).wait_for_publish(timeout=2)
        client.loop_stop()
        client.disconnect()
    print(f"sent {seq} telemetry, {alert_seq} alerts")
    return 0


def main(argv: list[str] | None = None) -> int:
    return run(parse_args(argv))


if __name__ == "__main__":
    sys.exit(main())
