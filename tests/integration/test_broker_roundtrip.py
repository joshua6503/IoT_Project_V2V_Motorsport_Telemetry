"""Simulator -> Mosquitto -> subscriber, payload validated against the schema.

Skipped when no broker is reachable. Start one with:
    docker compose -f gateway/docker-compose.yml up -d
Override the address with the V2V_BROKER environment variable (host[:port]).
"""
from __future__ import annotations

import json
import os
import queue
import socket
import subprocess
import sys

import paho.mqtt.client as mqtt
import pytest

from conftest import ROOT

BROKER = os.environ.get("V2V_BROKER", "localhost:1883")
HOST, _, PORT = BROKER.partition(":")
PORT = int(PORT or 1883)
NODE_ID = "sim-it"


def broker_up() -> bool:
    try:
        with socket.create_connection((HOST, PORT), timeout=1):
            return True
    except OSError:
        return False


pytestmark = pytest.mark.skipif(not broker_up(), reason=f"no MQTT broker at {BROKER}")


def test_simulator_telemetry_reaches_broker_and_is_valid(validator_for):
    received: queue.Queue = queue.Queue()
    sub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="pytest-sub")
    sub.on_message = lambda _c, _u, msg: received.put(msg)
    sub.connect(HOST, PORT)
    sub.subscribe(f"v2v/{NODE_ID}/telemetry", qos=0)
    sub.loop_start()
    try:
        subprocess.run(
            [sys.executable, str(ROOT / "tools" / "node-simulator" / "simulator.py"),
             "--broker", BROKER, "--node-id", NODE_ID, "--rate", "20", "--count", "10"],
            check=True, timeout=30,
        )
        msg = received.get(timeout=5)
    finally:
        sub.loop_stop()
        sub.disconnect()

    assert msg.topic == f"v2v/{NODE_ID}/telemetry"
    validator_for("telemetry").validate(json.loads(msg.payload))
