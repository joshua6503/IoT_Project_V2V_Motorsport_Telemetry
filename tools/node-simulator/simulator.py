#!/usr/bin/env python3
"""Fake V2V nodes: publish telemetry, status (with LWT) and occasional alerts over MQTT.

Topics and payloads follow interfaces/mqtt-topics.md and
interfaces/payload.schema.json. Values are random but schema-valid.
Broker address and credentials come from CLI args or environment variables
(V2V_BROKER, V2V_MQTT_USER, V2V_MQTT_PASSWORD); nothing is hard-coded.
"""
from __future__ import annotations

import argparse
import getpass
import json
import os
import random
import sys
import time

import paho.mqtt.client as mqtt

ALERT_TYPES = ["manual", "hard_brake", "obstacle", "overtemp"]
SEVERITIES = ["info", "warning", "critical"]
UINT32_MAX = 0xFFFFFFFF
UINT16_MAX = 0xFFFF
SENSOR_DIST, SENSOR_TEMP, SENSOR_BRAKE = 0x01, 0x02, 0x04
DEFAULT_NODES = "rw612-a,esp32-b"


def topic(node_id: str, leaf: str) -> str:
    return f"v2v/{node_id}/{leaf}"


def make_telemetry(node_id: str, seq: int, ts_ms: int, rng: random.Random) -> dict:
    no_echo = rng.random() < 0.05
    temp_fault = rng.random() < 0.01
    sensor_ok = SENSOR_BRAKE
    if not no_echo:
        sensor_ok |= SENSOR_DIST
    if not temp_fault:
        sensor_ok |= SENSOR_TEMP
    return {
        "node_id": node_id,
        "ts_ms": ts_ms & UINT32_MAX,
        "seq": seq & UINT32_MAX,
        "brake_pct": round(min(100.0, max(0.0, rng.gauss(30.0, 25.0))), 1),
        "tps_pct": round(rng.uniform(0.0, 100.0), 1),
        "temp_c": None if temp_fault else round(rng.uniform(25.0, 95.0), 1),
        "distance_cm": None if no_echo else round(rng.uniform(20.0, 400.0), 1),
        "sensor_ok": sensor_ok,
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
    p.add_argument("--broker", default=os.environ.get("V2V_BROKER", "localhost:1883"),
                   help="broker as host[:port] (env V2V_BROKER, default: %(default)s)")
    p.add_argument("--username", default=os.environ.get("V2V_MQTT_USER"),
                   help="MQTT user (env V2V_MQTT_USER). Same user for all simulated nodes.")
    p.add_argument("--password", default=os.environ.get("V2V_MQTT_PASSWORD"),
                   help="MQTT password (env V2V_MQTT_PASSWORD); prompted (hidden) if a user "
                        "is set and no password is given")
    p.add_argument("--nodes", default=DEFAULT_NODES,
                   help="comma-separated node ids (default: %(default)s)")
    p.add_argument("--node-id", default=None, help="single node id (same as --nodes X)")
    p.add_argument("--rate", type=float, default=5.0,
                   help="telemetry rate in Hz per node (default: %(default)s)")
    p.add_argument("--alert-prob", type=float, default=0.02,
                   help="probability of an alert per telemetry sample (default: %(default)s)")
    p.add_argument("--count", type=int, default=0,
                   help="stop after N telemetry messages per node, 0 = forever (default: %(default)s)")
    p.add_argument("--seed", type=int, default=None, help="random seed for reproducible runs")
    args = p.parse_args(argv)
    if args.rate <= 0:
        p.error("--rate must be > 0")
    if args.node_id:
        args.nodes = args.node_id
    args.node_list = [n.strip() for n in args.nodes.split(",") if n.strip()]
    if not args.node_list:
        p.error("no node ids given")
    return args


def make_client(node: str, host: str, port: int, args: argparse.Namespace) -> mqtt.Client:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"{node}-simulator")
    if args.username:
        client.username_pw_set(args.username, args.password)
    client.will_set(topic(node, "status"), json.dumps(make_status(node, False)), qos=1, retain=True)

    def on_connect(c, _userdata, _flags, reason_code, _props):
        print(f"[{node}] connected to {host}:{port} ({reason_code})")
        if not reason_code.is_failure:
            c.publish(topic(node, "status"), json.dumps(make_status(node, True)), qos=1, retain=True)
            c.subscribe(topic(node, "cmd"), qos=1)

    def on_message(_c, _userdata, msg):
        print(f"[{node}] cmd on {msg.topic}: {msg.payload!r} (commands are TBD, ignored)")

    client.on_connect = on_connect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=10)
    client.connect(host, port, keepalive=30)
    client.loop_start()
    return client


def run(args: argparse.Namespace) -> int:
    rng = random.Random(args.seed)
    host, port = parse_broker(args.broker)
    if args.username and args.password is None:
        args.password = getpass.getpass(f"MQTT password for {args.username}: ")

    clients = {n: make_client(n, host, port, args) for n in args.node_list}
    period = 1.0 / args.rate
    t0 = time.monotonic()
    seq = 0
    alert_seq = {n: 0 for n in args.node_list}
    try:
        while args.count == 0 or seq < args.count:
            ts_ms = int((time.monotonic() - t0) * 1000)
            for node, client in clients.items():
                client.publish(topic(node, "telemetry"),
                               json.dumps(make_telemetry(node, seq, ts_ms, rng)), qos=0)
                if rng.random() < args.alert_prob:
                    alert = make_alert(node, alert_seq[node], ts_ms, rng)
                    client.publish(topic(node, "alert"), json.dumps(alert), qos=1)
                    print(f"alert: {alert}")
                    alert_seq[node] += 1
            seq += 1
            time.sleep(period)
    except KeyboardInterrupt:
        pass
    finally:
        # A clean disconnect does not fire the LWT, so publish offline explicitly.
        for node, client in clients.items():
            client.publish(topic(node, "status"), json.dumps(make_status(node, False)),
                           qos=1, retain=True).wait_for_publish(timeout=2)
            client.loop_stop()
            client.disconnect()
    print(f"sent {seq} telemetry per node, alerts {alert_seq}")
    return 0


def main(argv: list[str] | None = None) -> int:
    return run(parse_args(argv))


if __name__ == "__main__":
    sys.exit(main())
