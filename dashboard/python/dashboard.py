#!/usr/bin/env python3
"""Simple Tkinter dashboard for MP2: shows both V2V nodes and sends commands (DEC-013..015).

Gauges and live charts are fed by the per-variable topics v2v/+/sensor/{brake_pct|temp_c|distance_cm}
(plain decimal text, "null" on a sensor fault). v2v/+/telemetry, v2v/+/alert and v2v/+/status still
drive seq/last-seen, the online state and the alert log. Commands go to
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
import math
import time
from collections import deque
from datetime import datetime

import paho.mqtt.client as mqtt

try:
    import tkinter as tk
    from tkinter import ttk
except ImportError:  # headless Python without Tk: builders and parsing still importable
    tk = None
    ttk = None

try:
    import matplotlib
    matplotlib.use("TkAgg")
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    from matplotlib.figure import Figure
    from matplotlib.patches import Wedge
except ImportError:  # parsing helpers stay importable without matplotlib
    FigureCanvasTkAgg = Figure = Wedge = None

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
SENSOR_VARS = ("brake_pct", "temp_c", "distance_cm")   # v2v/{node}/sensor/{var}
CHART_WINDOW_S = 300        # live charts show the last 5 minutes
STALE_S = 5.0               # a gauge shows "--" when its value is older than this
REDRAW_MS = 500             # throttled redraw (~2 Hz)
GREEN, YELLOW, RED = "green", "yellow", "red"
# key -> (title, unit, min, max, text shown for null). Bands use the firmware bench defaults
# (alert_config_default, OPEN-005: calibrate): brake hard at 70 %, temp 30/35 C, obstacle 30/15 cm.
GAUGE_SPECS = {
    "brake_pct": ("Freno", "%", 0.0, 100.0, "--"),
    "temp_c": ("Temperatura", "°C", 0.0, 60.0, "--"),
    "distance_cm": ("Distancia", "cm", 0.0, 200.0, "sin eco"),
}
BAND_COLORS = {GREEN: "#2e9e4f", YELLOW: "#f0b400", RED: "#d62728"}
NODE_COLORS = ("#1f5fbf", "#d9630a", "#2e9e4f", "#8e44ad")


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


def parse_sensor(topic: str, payload: bytes) -> tuple[str, str, float | None] | None:
    """Parse v2v/{node}/sensor/{var}: (node_id, var, value). value is None for the text "null".

    Returns None for any other topic, a reserved node, or text that is not a finite number.
    """
    parts = topic.split("/")
    if (len(parts) != 4 or parts[0] != "v2v" or parts[2] != "sensor"
            or parts[3] not in SENSOR_VARS or parts[1] in (ALL_TARGET, "")):
        return None
    try:
        text = payload.decode("utf-8").strip()
    except UnicodeDecodeError:
        return None
    if text == "null":
        return parts[1], parts[3], None
    try:
        value = float(text)
    except ValueError:
        return None
    return (parts[1], parts[3], value) if math.isfinite(value) else None


def gauge_band(var: str, value: float) -> str:
    """GREEN/YELLOW/RED for a value. Distance is inverted: near is dangerous."""
    if var == "brake_pct":
        return RED if value >= 70.0 else YELLOW if value >= 50.0 else GREEN
    if var == "temp_c":
        return RED if value >= 35.0 else YELLOW if value >= 30.0 else GREEN
    if var == "distance_cm":
        return RED if value < 15.0 else YELLOW if value < 30.0 else GREEN
    raise KeyError(var)


def gauge_text(var: str, value: float | None) -> str:
    """Big number under a gauge: "42.5 %", or the null text when there is no value."""
    _title, unit, _lo, _hi, null_text = GAUGE_SPECS[var]
    return null_text if value is None else f"{value:.1f} {unit}"


def gauge_fraction(var: str, value: float) -> float:
    """Needle position 0..1 along the dial, clamped to the gauge range."""
    _t, _u, lo, hi, _n = GAUGE_SPECS[var]
    return min(1.0, max(0.0, (value - lo) / (hi - lo)))


class SeriesStore:
    """Time series per (node, var) for the live charts. None (null) is stored as NaN: a line gap."""

    def __init__(self, window_s: float = CHART_WINDOW_S):
        self.window_s = window_s
        self.series: dict[tuple[str, str], deque] = {}

    def add(self, node: str, var: str, value: float | None, now: float) -> None:
        q = self.series.setdefault((node, var), deque())
        q.append((now, math.nan if value is None else value))
        self._prune(q, now)

    def _prune(self, q: deque, now: float) -> None:
        while q and now - q[0][0] > self.window_s:
            q.popleft()

    def points(self, node: str, var: str, now: float) -> tuple[list[float], list[float]]:
        """(seconds ago as negative numbers, values) inside the window."""
        q = self.series.get((node, var))
        if not q:
            return [], []
        self._prune(q, now)
        return [t - now for t, _v in q], [v for _t, v in q]


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
    """One LabelFrame per node: online state, last-message age and three circular gauges."""

    def __init__(self, parent, node_id: str):
        self.node_id = node_id
        self.last_seen: float | None = None   # time.monotonic() of the last non-retained message
        self.latest: dict[str, tuple[float, float | None]] = {}   # var -> (monotonic, value or None)
        self.frame = ttk.LabelFrame(parent, text=f" {node_id} ", padding=4)
        top = ttk.Frame(self.frame)
        top.pack(fill="x")
        self.status = tk.Label(top, text="Sin estado", font=("Arial", 14, "bold"),
                               width=14, bg="gray", fg="white")
        self.status.pack(side="left")
        self.seen = tk.StringVar(value="Último mensaje: sin datos")
        ttk.Label(top, textvariable=self.seen, font=("Arial", 13)).pack(side="left", padx=12)

        self.fig = Figure(figsize=(7.2, 2.1), dpi=100, facecolor="white")
        self.needles, self.texts = {}, {}
        for i, var in enumerate(SENSOR_VARS):
            ax = self.fig.add_subplot(1, 3, i + 1)
            title, _unit, lo, hi, _null = GAUGE_SPECS[var]
            ax.set_xlim(-1.25, 1.25)
            ax.set_ylim(-0.45, 1.25)
            ax.set_aspect("equal")
            ax.axis("off")
            ax.set_title(title, fontsize=15, fontweight="bold", color="#111111", pad=2)
            for b_lo, b_hi, color in self._bands(var):
                f_lo = gauge_fraction(var, b_lo)
                f_hi = gauge_fraction(var, b_hi)
                ax.add_patch(Wedge((0, 0), 1.0, 180 * (1 - f_hi), 180 * (1 - f_lo), width=0.32,
                                   facecolor=BAND_COLORS[color], edgecolor="white", linewidth=1.5))
            (needle,) = ax.plot([], [], color="#111111", linewidth=3.5, solid_capstyle="round")
            ax.add_patch(Wedge((0, 0), 0.07, 0, 360, facecolor="#111111"))
            ax.text(-1.0, -0.16, f"{lo:g}", ha="center", fontsize=10, color="#444444")
            ax.text(1.0, -0.16, f"{hi:g}", ha="center", fontsize=10, color="#444444")
            self.texts[var] = ax.text(0, -0.36, "--", ha="center", va="center", fontsize=19,
                                      fontweight="bold", color="#777777")
            self.needles[var] = needle
        self.fig.subplots_adjust(left=0.01, right=0.99, top=0.88, bottom=0.02, wspace=0.05)
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.frame)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

    @staticmethod
    def _bands(var: str):
        """(low, high, color) segments of the dial; same thresholds as gauge_band()."""
        _t, _u, lo, hi, _n = GAUGE_SPECS[var]
        cuts = {"brake_pct": (50.0, 70.0), "temp_c": (30.0, 35.0), "distance_cm": (15.0, 30.0)}[var]
        colors = (RED, YELLOW, GREEN) if var == "distance_cm" else (GREEN, YELLOW, RED)
        edges = (lo, *cuts, hi)
        return [(edges[i], edges[i + 1], colors[i]) for i in range(3)]

    def set_online(self, online: bool) -> None:
        if online:
            self.status.config(text="En línea", bg="green")
        else:
            self.status.config(text="Fuera de línea", bg="gray")

    def set_value(self, var: str, value: float | None, now: float) -> None:
        self.latest[var] = (now, value)

    def refresh_gauges(self, now: float) -> None:
        for var in SENSOR_VARS:
            stamp, value = self.latest.get(var, (None, None))
            if stamp is None or now - stamp > STALE_S:
                value = None
            if value is None:
                self.needles[var].set_data([], [])
                self.texts[var].set_text(gauge_text(var, None))
                self.texts[var].set_color("#777777")
            else:
                theta = math.pi * (1.0 - gauge_fraction(var, value))
                self.needles[var].set_data([0, 0.86 * math.cos(theta)], [0, 0.86 * math.sin(theta)])
                self.texts[var].set_text(gauge_text(var, value))
                self.texts[var].set_color("#111111")
        self.canvas.draw_idle()

    def tick(self) -> None:
        age = None if self.last_seen is None else time.monotonic() - self.last_seen
        self.seen.set(f"Último mensaje: {fmt_age(age)}")


class DashboardApp:
    def __init__(self, root, client: mqtt.Client, node_list: list[str], broker_text: str, user: str):
        if tk is None:
            raise RuntimeError("tkinter is not available in this Python")
        if Figure is None:
            raise RuntimeError("matplotlib is not installed (pip install -r dashboard/python/requirements.txt)")
        self.root = root
        self.client = client
        self.events: queue.Queue = queue.Queue()
        self.cards: dict[str, NodeCard] = {}
        self.known: set[str] = set(node_list)
        self.configured = list(node_list)
        self.store = SeriesStore()
        self.lines: dict[tuple[str, str], object] = {}
        self.cmd_seq = int(time.time()) & UINT32_MASK   # new run -> new range, not deduped by nodes
        self.relayed: set[tuple[str, int]] = set()      # (source node, alert seq) already relayed
        self.banner_job = None
        self.broker_text, self.user = broker_text, user

        root.title("V2V Motorsport — Dashboard MP2")
        root.geometry("1400x860")
        style = ttk.Style()
        style.configure(".", font=("Arial", 12))
        style.configure("Treeview", rowheight=26, font=("Arial", 12))
        style.configure("Treeview.Heading", font=("Arial", 12, "bold"))
        style.configure("TLabelframe.Label", font=("Arial", 13, "bold"))
        self.conn = tk.Label(root, text="● Conectando...", fg="gray", anchor="w", font=("Arial", 12))
        self.conn.pack(fill="x", padx=10, pady=(6, 0))
        self.banner = tk.Label(root, text="Sin alerta crítica activa", font=("Arial", 20, "bold"),
                               bg="#e6e6e6", fg="#444444", pady=6)
        self.banner.pack(fill="x", padx=10, pady=6)

        bottom = ttk.Frame(root)
        bottom.pack(side="bottom", fill="x", padx=10, pady=(6, 10))   # packed first: never squeezed out
        self._build_command_panel(bottom)
        self._build_log(bottom)

        body = ttk.Frame(root)
        body.pack(fill="both", expand=True, padx=10)
        self.nodes_frame = ttk.Frame(body)
        self.nodes_frame.pack(side="left", fill="both", expand=True)
        for node in self.configured:
            self._ensure_card(node)
        self._build_charts(body)

        client.on_connect = self._on_connect
        client.on_disconnect = self._on_disconnect
        client.on_message = self._on_message
        self.root.after(100, self._drain)
        self.root.after(1000, self._tick)
        self.root.after(REDRAW_MS, self._redraw)

    # -- widgets -----------------------------------------------------------

    def _ensure_card(self, node: str) -> NodeCard:
        if node not in self.cards:
            card = NodeCard(self.nodes_frame, node)
            card.frame.pack(side="top", fill="both", expand=True, pady=2)
            self.cards[node] = card
        self.known.add(node)
        return self.cards[node]

    def _node_color(self, node: str) -> str:
        order = self.configured + sorted(n for n in self.known if n not in self.configured)
        return NODE_COLORS[order.index(node) % len(NODE_COLORS)]

    def _build_charts(self, parent) -> None:
        frame = ttk.LabelFrame(parent, text=" Últimos 5 minutos ", padding=4)
        frame.pack(side="left", fill="both", expand=True, padx=(8, 0), pady=2)
        self.chart_fig = Figure(figsize=(6.0, 6.2), dpi=100, facecolor="white")
        self.chart_axes = {}
        for i, var in enumerate(SENSOR_VARS):
            title, unit, lo, hi, _n = GAUGE_SPECS[var]
            ax = self.chart_fig.add_subplot(3, 1, i + 1)
            ax.set_facecolor("white")
            ax.set_title(title, fontsize=14, fontweight="bold", loc="left", pad=3)
            ax.set_ylabel(unit, fontsize=12)
            ax.set_xlim(-CHART_WINDOW_S, 0)
            ax.set_ylim(lo, hi)
            ax.grid(True, color="#cccccc", linewidth=0.8)
            ax.tick_params(labelsize=11)
            if i < 2:
                ax.tick_params(labelbottom=False)
            else:
                ax.set_xlabel("segundos", fontsize=12)
            self.chart_axes[var] = ax
        self.chart_fig.subplots_adjust(left=0.1, right=0.97, top=0.95, bottom=0.11, hspace=0.35)
        self.chart_canvas = FigureCanvasTkAgg(self.chart_fig, master=frame)
        self.chart_canvas.get_tk_widget().pack(fill="both", expand=True)

    def _build_command_panel(self, parent) -> None:
        panel = ttk.LabelFrame(parent, text=" Comandos ", padding=8)
        panel.pack(side="left", fill="y", padx=(0, 8))
        ttk.Label(panel, text="Destino:").grid(row=0, column=0, sticky="w", pady=2)
        self.target = ttk.Combobox(panel, state="readonly", width=12,
                                   values=self.configured + [ALL_LABEL])
        self.target.set(ALL_LABEL)
        self.target.grid(row=0, column=1, padx=6)
        ttk.Label(panel, text="Severidad:").grid(row=1, column=0, sticky="w", pady=2)
        self.severity = ttk.Combobox(panel, state="readonly", width=12, values=["warning", "critical"])
        self.severity.set("warning")
        self.severity.grid(row=1, column=1, padx=6)
        ttk.Label(panel, text="Duración (ms):").grid(row=2, column=0, sticky="w", pady=2)
        self.duration = tk.StringVar(value=str(DEFAULT_DURATION_MS))
        ttk.Spinbox(panel, from_=MIN_DURATION_MS, to=MAX_DURATION_MS, increment=100, width=11,
                    textvariable=self.duration).grid(row=2, column=1, padx=6)
        ttk.Button(panel, text="Enviar alerta", command=self.send_alert).grid(row=3, column=0, pady=6)
        ttk.Button(panel, text="Apagar alerta", command=self.send_clear).grid(row=3, column=1)
        self.relay = tk.BooleanVar(value=False)
        ttk.Checkbutton(panel, text="Reenviar frenado brusco\na los demás nodos",
                        variable=self.relay).grid(row=4, column=0, columnspan=2, sticky="w", pady=(4, 0))

    def _build_log(self, parent) -> None:
        frame = ttk.LabelFrame(parent, text=" Registro de alertas y comandos ", padding=4)
        frame.pack(side="left", fill="both", expand=True)
        self.log = ttk.Treeview(frame, columns=("hora", "nodo", "tipo", "sev"), show="headings", height=5)
        for col, title, width in (("hora", "Hora", 90), ("nodo", "Nodo", 140),
                                  ("tipo", "Tipo", 300), ("sev", "Severidad", 110)):
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
        subs = [(f"v2v/+/{leaf}", 0 if leaf == "telemetry" else 1) for leaf in LEAVES]
        subs.append(("v2v/+/sensor/+", 0))
        client.subscribe(subs)
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

    def _redraw(self) -> None:
        """Throttled (~2 Hz) redraw of the gauges and the charts."""
        now = time.monotonic()
        for card in self.cards.values():
            card.refresh_gauges(now)
        for var, ax in self.chart_axes.items():
            for node in sorted(self.known):
                xs, ys = self.store.points(node, var, now)
                line = self.lines.get((node, var))
                if line is None:
                    if not xs:
                        continue
                    (line,) = ax.plot([], [], color=self._node_color(node), linewidth=2.2, label=node)
                    self.lines[(node, var)] = line
                    ax.legend(loc="upper left", fontsize=11, framealpha=0.9)
                line.set_data(xs, ys)
        self.chart_canvas.draw_idle()
        self.root.after(REDRAW_MS, self._redraw)

    def _show_conn(self, ok: bool, reason: str) -> None:
        if ok:
            self.conn.config(text=f"● Conectado a {self.broker_text} como {self.user}", fg="green")
        elif reason and "Success" not in reason:
            self.conn.config(text=f"● Sin conexión: {reason} (reintentando)", fg="red")
        else:
            self.conn.config(text="● Desconectado", fg="red")

    def _handle_message(self, topic: str, payload: bytes, retained: bool) -> None:
        sensor = parse_sensor(topic, payload)
        if sensor is not None:
            node, var, value = sensor
            card = self._ensure_card(node)
            now = time.monotonic()
            card.last_seen = now
            card.set_value(var, value, now)
            self.store.add(node, var, value, now)
            return
        parsed = parse_message(topic, payload)
        if parsed is None:
            print(f"ignored message on {topic}: not a valid message for this dashboard")
            return
        leaf, node, data = parsed
        card = self._ensure_card(node)
        if not retained:
            card.last_seen = time.monotonic()
            card.tick()
        if leaf == "status":
            if data.get("state") in ("online", "offline"):
                card.set_online(data["state"] == "online")
        elif leaf == "alert":
            self._handle_alert(node, data)
        # telemetry: only seq / last-seen here; values come from the sensor/ topics

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
    if Figure is None:
        print("Falta matplotlib: pip install -r dashboard/python/requirements.txt", file=sys.stderr)
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
