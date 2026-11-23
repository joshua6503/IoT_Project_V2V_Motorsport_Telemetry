"""Simulator -> Mosquitto -> subscriber, payloads validated against the schema.

Skipped when no broker is reachable. Address and credentials come from the environment:
    V2V_BROKER=host[:port]  V2V_MQTT_USER=...  V2V_MQTT_PASSWORD=...
Without V2V_MQTT_USER the connection is anonymous (laptop dev broker).
"""
from __future__ import annotations

import json
import os
import queue
import socket
import subprocess
import sys
import time

import paho.mqtt.client as mqtt
import pytest

from conftest import ROOT

BROKER = os.environ.get("V2V_BROKER", "localhost:1883")
HOST, _, PORT = BROKER.partition(":")
PORT = int(PORT or 1883)
USER = os.environ.get("V2V_MQTT_USER")
PASSWORD = os.environ.get("V2V_MQTT_PASSWORD")
NODES = ["sim-it1", "sim-it2"]


def broker_up() -> bool:
    try:
        with socket.create_connection((HOST, PORT), timeout=1):
            return True
    except OSError:
        return False


pytestmark = pytest.mark.skipif(not broker_up(), reason=f"no MQTT broker at {BROKER}")


def test_simulator_two_nodes_reach_broker_and_are_valid(validator_for):
    received: queue.Queue = queue.Queue()
    sub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="pytest-sub")
    if USER:
        sub.username_pw_set(USER, PASSWORD)
    sub.on_message = lambda _c, _u, msg: received.put(msg)
    sub.connect(HOST, PORT)
    sub.subscribe("v2v/+/telemetry", qos=0)
    sub.subscribe("v2v/+/status", qos=1)
    sub.loop_start()
    try:
        env = {**os.environ, "V2V_BROKER": BROKER}
        subprocess.run(
            [sys.executable, str(ROOT / "tools" / "node-simulator" / "simulator.py"),
             "--nodes", ",".join(NODES), "--rate", "20", "--count", "10"],
            check=True, timeout=30, env=env,
        )
        deadline = time.monotonic() + 5
        telemetry_nodes, status = set(), {}
        while time.monotonic() < deadline and not (set(NODES) <= telemetry_nodes
                                                    and all(status.get(n) == "offline" for n in NODES)):
            try:
                msg = received.get(timeout=1)
            except queue.Empty:
                continue
            body = json.loads(msg.payload)
            leaf = msg.topic.split("/")[-1]
            node = msg.topic.split("/")[1]
            if node not in NODES:
                continue
            if leaf == "telemetry":
                validator_for("telemetry").validate(body)
                assert body["node_id"] == node
                telemetry_nodes.add(node)
            elif leaf == "status":
                validator_for("status").validate(body)
                status[node] = body["state"]
    finally:
        sub.loop_stop()
        sub.disconnect()

    assert telemetry_nodes == set(NODES)
    # the retained status ends as "offline" after the simulator exits cleanly
    assert all(status.get(n) == "offline" for n in NODES)
