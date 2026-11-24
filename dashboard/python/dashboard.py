#!/usr/bin/env python3
"""Simple Tkinter dashboard for MP2: shows both V2V nodes and sends commands (DEC-013..015).

Subscribes to v2v/+/telemetry, v2v/+/alert and v2v/+/status, and publishes commands to
v2v/{node_id}/cmd or v2v/all/cmd. Topics and payloads follow interfaces/mqtt-topics.md and
interfaces/payload.schema.json. The UI is in Spanish; code and comments are in English.

Broker and credentials come from CLI args or environment variables (V2V_BROKER, V2V_MQTT_USER,
V2V_MQTT_PASSWORD). The password is never stored: it comes from the environment or a hidden prompt.
"""
from __future__ import annotations

import argparse
import getpass
import json
import os
import queue
import random
import sys
import time
from datetime import datetime

import paho.mqtt.client as mqtt

try:
    import tkinter as tk
    from tkinter import ttk
except ImportError:  # headless Python without Tk: builders and parsing still importable
    tk = None
    ttk = None

DEFAULT_BROKER = "192.168.0.239:1883"
DEFAULT_USER = "dashboard"
DEFAULT_NODES = "rw612-a,esp32-b"
ORIGIN = "dashboard"
ALL_TARGET = "all"          # reserved node_id (v2v/all/cmd)
ALL_LABEL = "todos"         # what the UI shows for ALL_TARGET
DEFAULT_DURATION_MS = 3000
MIN_DURATION_MS, MAX_DURATION_MS = 100, 10000
UINT32_MASK = 0xFFFFFFFF
BANNER_MS = 3000            # red banner time after a critical alert (= default command duration)
MAX_LOG_ROWS = 200
LEAVES = ("telemetry", "alert", "status")


# ---------------------------------------------------------------------------
# Pure helpers (no Tk, no network): tested in tests/integration/test_commands.py
# ---------------------------------------------------------------------------

def cmd_topic(target: str) -> str:
    """Topic for a command target: a node id, or "todos"/"all" for every node."""
    node = ALL_TARGET if target in (ALL_LABEL, ALL_TARGET) else target
    return f"v2v/{node}/cmd"


def build_alert_cmd(seq: int, alert_type: str, severity: str,
                    duration_ms: int = DEFAULT_DURATION_MS,
                    source_node: str | None = None, origin: str = ORIGIN) -> dict:
    if not MIN_DURATION_MS <= duration_ms <= MAX_DURATION_MS:
        raise ValueError(f"duration_ms must be {MIN_DURATION_MS}..{MAX_DURATION_MS}")
    cmd = {
        "cmd": "alert",
        "seq": seq & UINT32_MASK,
        "origin": origin,
        "alert_type": alert_type,
        "severity": severity,
        "duration_ms": duration_ms,
    }
    if source_node:
        cmd["source_node"] = source_node
    return cmd


def build_clear_cmd(seq: int, origin: str = ORIGIN) -> dict:
    return {"cmd": "clear", "seq": seq & UINT32_MASK, "origin": origin}


def relay_targets(source: str, known: set[str] | list[str]) -> list[str]:
    """Nodes that receive a relayed alert from `source`: every known node except the source."""
    return sorted(n for n in known if n != source and n != ALL_TARGET)


def parse_message(topic: str, payload: bytes) -> tuple[str, str, dict] | None:
    """Return (leaf, node_id, data) for a v2v/{node}/{telemetry|alert|status} JSON object, else None."""
    parts = topic.split("/")
    if len(parts) != 3 or parts[0] != "v2v" or parts[2] not in LEAVES or parts[1] == ALL_TARGET:
        return None
    try:
        data = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, ValueError):
        return None
    return (parts[2], parts[1], data) if isinstance(data, dict) else None


def parse_broker(value: str) -> tuple[str, int]:
    host, _, port = value.partition(":")
    return host or "localhost", int(port) if port else 1883


def fmt_age(seconds: float | None) -> str:
    if seconds is None:
        return "sin datos"
    if seconds < 60:
        return f"hace {int(seconds)} s"
    return f"hace {int(seconds // 60)} min"


def fmt_value(data: dict, key: str, unit: str, null_text: str = "--") -> str | None:
    """Text for a telemetry field; None when the key is absent (keep the previous text)."""
    if key not in data:
        return None
    value = data[key]
    if value is None:
        return null_text
    return f"{value:.1f} {unit}" if isinstance(value, (int, float)) else null_text


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--broker", default=os.environ.get("V2V_BROKER", DEFAULT_BROKER),
                   help="broker as host[:port] (env V2V_BROKER, default: %(default)s)")
    p.add_argument("--username", default=os.environ.get("V2V_MQTT_USER", DEFAULT_USER),
                   help="MQTT user (env V2V_MQTT_USER, default: %(default)s)")
    p.add_argument("--nodes", default=DEFAULT_NODES,
                   help="comma-separated node ids shown from the start (default: %(default)s)")
    args = p.parse_args(argv)
    args.node_list = [n.strip() for n in args.nodes.split(",") if n.strip()]
    if not args.node_list:
        p.error("no node ids given")
    return args


# ---------------------------------------------------------------------------
# GUI
# ---------------------------------------------------------------------------

class NodeCard:
    """One LabelFrame with the live values of a node."""

    def __init__(self, parent, node_id: str):
        self.node_id = node_id
        self.last_seen: float | None = None   # time.monotonic() of the last non-retained message
        self.frame = ttk.LabelFrame(parent, text=f" {node_id} ", padding=8)
        self.status = tk.Label(self.frame, text="Sin estado", font=("Arial", 13, "bold"),
                               width=16, bg="gray", fg="white")
        self.status.grid(row=0, column=0, columnspan=2, pady=(0, 6))
        self.seen = tk.StringVar(value="Último mensaje: sin datos")
        ttk.Label(self.frame, textvariable=self.seen).grid(row=1, column=0, columnspan=2, sticky="w")
        self.values = {}
        for row, (key, label) in enumerate((("brake", "Freno"), ("temp", "Temperatura"),
                                            ("dist", "Distancia")), start=2):
            ttk.Label(self.frame, text=f"{label}:").grid(row=row, column=0, sticky="w", pady=2)
            var = tk.StringVar(value="--")
            ttk.Label(self.frame, textvariable=var, font=("Arial", 12)).grid(row=row, column=1, sticky="e")
            self.values[key] = var

    def set_online(self, online: bool) -> None:
        if online:
            self.status.config(text="En línea", bg="green")
        else:
            self.status.config(text="Fuera de línea", bg="gray")

    def apply_telemetry(self, data: dict) -> None:
        for key, field, unit, null_text in (("brake", "brake_pct", "%", "--"),
                                            ("temp", "temp_c", "°C", "--"),
                                            ("dist", "distance_cm", "cm", "sin eco")):
            text = fmt_value(data, field, unit, null_text)
            if text is not None:
                self.values[key].set(text)

    def tick(self) -> None:
        age = None if self.last_seen is None else time.monotonic() - self.last_seen
        self.seen.set(f"Último mensaje: {fmt_age(age)}")


class DashboardApp:
    def __init__(self, root, client: mqtt.Client, node_list: list[str], broker_text: str, user: str):
        if tk is None:
            raise RuntimeError("tkinter is not available in this Python")
        self.root = root
        self.client = client
        self.events: queue.Queue = queue.Queue()
        self.cards: dict[str, NodeCard] = {}
        self.known: set[str] = set(node_list)
        self.configured = list(node_list)
        self.cmd_seq = int(time.time()) & UINT32_MASK   # new run -> new range, not deduped by nodes
        self.relayed: set[tuple[str, int]] = set()      # (source node, alert seq) already relayed
        self.banner_job = None
        self.broker_text, self.user = broker_text, user

        root.title("V2V Motorsport — Dashboard MP2")
        root.geometry("900x700")
        self.conn = tk.Label(root, text="● Conectando...", fg="gray", anchor="w")
        self.conn.pack(fill="x", padx=10, pady=(6, 0))
        self.banner = tk.Label(root, text="Sin alerta crítica activa", font=("Arial", 16, "bold"),
                               bg="#e6e6e6", fg="#444444", pady=8)
        self.banner.pack(fill="x", padx=10, pady=6)

        self.nodes_frame = ttk.Frame(root)
        self.nodes_frame.pack(fill="x", padx=10)
        for node in self.configured:
            self._ensure_card(node)

        self._build_command_panel()
        self._build_log()

        client.on_connect = self._on_connect
        client.on_disconnect = self._on_disconnect
        client.on_message = self._on_message
        self.root.after(100, self._drain)
        self.root.after(1000, self._tick)

    # -- widgets -----------------------------------------------------------

    def _ensure_card(self, node: str) -> NodeCard:
        if node not in self.cards:
            card = NodeCard(self.nodes_frame, node)
            card.frame.pack(side="left", fill="y", expand=True, padx=4, pady=4)
            self.cards[node] = card
        self.known.add(node)
        return self.cards[node]

    def _build_command_panel(self) -> None:
        panel = ttk.LabelFrame(self.root, text=" Comandos ", padding=8)
        panel.pack(fill="x", padx=10, pady=6)
        ttk.Label(panel, text="Destino:").grid(row=0, column=0, sticky="w")
        self.target = ttk.Combobox(panel, state="readonly", width=12,
                                   values=self.configured + [ALL_LABEL])
        self.target.set(ALL_LABEL)
        self.target.grid(row=0, column=1, padx=6)
        ttk.Label(panel, text="Severidad:").grid(row=0, column=2, sticky="w")
        self.severity = ttk.Combobox(panel, state="readonly", width=10, values=["warning", "critical"])
        self.severity.set("warning")
        self.severity.grid(row=0, column=3, padx=6)
        ttk.Label(panel, text="Duración (ms):").grid(row=0, column=4, sticky="w")
        self.duration = tk.StringVar(value=str(DEFAULT_DURATION_MS))
        ttk.Spinbox(panel, from_=MIN_DURATION_MS, to=MAX_DURATION_MS, increment=100, width=7,
                    textvariable=self.duration).grid(row=0, column=5, padx=6)
        ttk.Button(panel, text="Enviar alerta", command=self.send_alert).grid(row=0, column=6, padx=6)
        ttk.Button(panel, text="Apagar alerta", command=self.send_clear).grid(row=0, column=7)
        self.relay = tk.BooleanVar(value=False)
        ttk.Checkbutton(panel, text="Reenviar frenado brusco a los demás nodos",
                        variable=self.relay).grid(row=1, column=0, columnspan=8, sticky="w", pady=(8, 0))

    def _build_log(self) -> None:
        frame = ttk.LabelFrame(self.root, text=" Registro de alertas y comandos ", padding=4)
        frame.pack(fill="both", expand=True, padx=10, pady=(0, 10))
        self.log = ttk.Treeview(frame, columns=("hora", "nodo", "tipo", "sev"), show="headings", height=10)
        for col, title, width in (("hora", "Hora", 90), ("nodo", "Nodo", 140),
                                  ("tipo", "Tipo", 180), ("sev", "Severidad", 110)):
            self.log.heading(col, text=title)
            self.log.column(col, width=width, anchor="w")
        self.log.tag_configure("critical", foreground="red")
        self.log.tag_configure("cmd", foreground="#1a4fa0")
        scroll = ttk.Scrollbar(frame, orient="vertical", command=self.log.yview)
        self.log.configure(yscrollcommand=scroll.set)
        self.log.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")

    def _log_row(self, node: str, kind: str, severity: str, tag: str = "") -> None:
        self.log.insert("", 0, values=(datetime.now().strftime("%H:%M:%S"), node, kind, severity),
                        tags=(tag,) if tag else ())
        rows = self.log.get_children()
        for old in rows[MAX_LOG_ROWS:]:
            self.log.delete(old)

    # -- MQTT callbacks (paho thread: only queue.put, never touch Tk) -------

    def _on_connect(self, client, _userdata, _flags, reason_code, _props):
        if reason_code.is_failure:
            self.events.put(("conn", False, str(reason_code)))
            return
        client.subscribe([(f"v2v/+/{leaf}", 0 if leaf == "telemetry" else 1) for leaf in LEAVES])
        self.events.put(("conn", True, ""))

    def _on_disconnect(self, _client, _userdata, _flags, reason_code, _props):
        self.events.put(("conn", False, str(reason_code)))

    def _on_message(self, _client, _userdata, msg):
        self.events.put(("msg", msg.topic, msg.payload, bool(msg.retain)))

    # -- GUI thread ----------------------------------------------------------

    def _drain(self) -> None:
        try:
            while True:
                event = self.events.get_nowait()
                if event[0] == "conn":
                    self._show_conn(event[1], event[2])
                else:
                    self._handle_message(event[1], event[2], event[3])
        except queue.Empty:
            pass
        self.root.after(100, self._drain)

    def _tick(self) -> None:
        for card in self.cards.values():
            card.tick()
        self.root.after(1000, self._tick)

    def _show_conn(self, ok: bool, reason: str) -> None:
        if ok:
            self.conn.config(text=f"● Conectado a {self.broker_text} como {self.user}", fg="green")
        elif reason and "Success" not in reason:
            self.conn.config(text=f"● Sin conexión: {reason} (reintentando)", fg="red")
        else:
            self.conn.config(text="● Desconectado", fg="red")

    def _handle_message(self, topic: str, payload: bytes, retained: bool) -> None:
        parsed = parse_message(topic, payload)
        if parsed is None:
            print(f"ignored message on {topic}: not a valid JSON object for this dashboard")
            return
        leaf, node, data = parsed
        card = self._ensure_card(node)
        if not retained:
            card.last_seen = time.monotonic()
            card.tick()
        if leaf == "status":
            if data.get("state") in ("online", "offline"):
                card.set_online(data["state"] == "online")
        elif leaf == "telemetry":
            card.apply_telemetry(data)
        else:
            self._handle_alert(node, data)

    def _handle_alert(self, node: str, data: dict) -> None:
        alert_type, severity = data.get("alert_type", "?"), data.get("severity", "?")
        self._log_row(node, alert_type, severity, "critical" if severity == "critical" else "")
        if severity == "critical":
            self._show_banner(f"ALERTA CRÍTICA — {node} / {alert_type}")
        key = (node, data.get("seq", -1))
        if self.relay.get() and alert_type == "hard_brake" and key not in self.relayed:
            self.relayed.add(key)
            for target in relay_targets(node, self.known):
                self._publish(target, build_alert_cmd(self._next_seq(), "hard_brake", "critical",
                                                      DEFAULT_DURATION_MS, source_node=node))

    def _show_banner(self, text: str) -> None:
        if self.banner_job is not None:
            self.root.after_cancel(self.banner_job)
        self.banner.config(text=text, bg="red", fg="white")
        self.banner_job = self.root.after(BANNER_MS, self._hide_banner)

    def _hide_banner(self) -> None:
        self.banner_job = None
        self.banner.config(text="Sin alerta crítica activa", bg="#e6e6e6", fg="#444444")

    # -- commands --------------------------------------------------------------

    def _next_seq(self) -> int:
        self.cmd_seq = (self.cmd_seq + 1) & UINT32_MASK
        return self.cmd_seq

    def _publish(self, target: str, cmd: dict) -> None:
        topic = cmd_topic(target)
        info = self.client.publish(topic, json.dumps(cmd), qos=1, retain=False)
        label = f"→ {target}"
        if cmd["cmd"] == "alert":
            relay = f" (de {cmd['source_node']})" if "source_node" in cmd else ""
            self._log_row(label, f"cmd alert {cmd['alert_type']}{relay}", cmd["severity"], "cmd")
        else:
            self._log_row(label, "cmd clear", "-", "cmd")
        if info.rc != mqtt.MQTT_ERR_SUCCESS:
            self._log_row(label, "no enviado (sin conexión)", "-", "critical")

    def send_alert(self) -> None:
        try:
            duration = int(self.duration.get())
            cmd = build_alert_cmd(self._next_seq(), "manual", self.severity.get(), duration)
        except ValueError:
            self._log_row("→ " + self.target.get(), "duración inválida (100 a 10000 ms)", "-", "critical")
            return
        self._publish(self.target.get(), cmd)

    def send_clear(self) -> None:
        self._publish(self.target.get(), build_clear_cmd(self._next_seq()))


def make_client(user: str, password: str | None) -> mqtt.Client:
    client_id = f"dashboard-py-{random.randrange(16 ** 4):04x}"
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    if user:
        client.username_pw_set(user, password)   # must come before connect
    client.reconnect_delay_set(min_delay=1, max_delay=10)
    return client


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if tk is None:
        print("Este Python no incluye tkinter.", file=sys.stderr)
        return 1
    password = os.environ.get("V2V_MQTT_PASSWORD")
    if args.username and password is None:
        password = getpass.getpass(f"Contraseña MQTT de {args.username}: ")
    host, port = parse_broker(args.broker)

    client = make_client(args.username, password)
    root = tk.Tk()
    DashboardApp(root, client, args.node_list, f"{host}:{port}", args.username)
    client.connect_async(host, port, keepalive=30)
    client.loop_start()

    def close() -> None:
        client.loop_stop()
        client.disconnect()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", close)
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
