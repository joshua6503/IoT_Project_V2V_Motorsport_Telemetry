# tests

See [`docs/04-testing/test-plan.md`](../docs/04-testing/test-plan.md) for the
strategy and the requirement → test traceability table.

| Folder | Level | Run |
|---|---|---|
| `unit/` | Unity tests of `firmware/common/` + C contract layout, host gcc | `make -C tests/unit` |
| `integration/` | pytest: schema checks + simulator → broker round trip | `pytest tests/integration -v` |
| `hil/` | manual hardware-in-the-loop checklist | [`hil/hil-checklist.md`](hil/hil-checklist.md) |

## Unit tests on Windows

The Makefile needs a host C compiler (`gcc`, or `CC=clang`) and `make`.
The Makefile clones Unity (pinned tag) into `tests/unit/vendor/` on first run
(git-ignored), so `git` must be on the PATH.

```powershell
make -C tests/unit            # or: make -C tests/unit CC=clang
```

### What works on the team's Windows machines today (no MSYS2/WSL needed)

Verified 2026-11-11 (47/47 tests pass): GNU Make 4.4.1 shipped with MCUXpresso IDE plus
Zig's C compiler from the `ziglang` pip package in the repo's `.venv`. From the repo root:

```powershell
# one time: Python venv with zig (and the integration-test deps)
py -3.12 -m venv .venv
.venv\Scripts\python -m pip install ziglang -r tests/integration/requirements.txt

# each new PowerShell session: put MCUXpresso's make.exe on the PATH
# (adjust the version folder to your MCUXpresso IDE install)
$env:Path = "C:\nxp\MCUXpressoIDE_25.6.136\ide\buildtools\bin;$env:Path"

# build and run; CC is relative to tests/unit because make -C changes directory
make -C tests/unit CC="../../.venv/Scripts/python-zig.exe cc"
make -C tests/unit clean
```

### ESP32 core version

CI installs the **latest** `esp32:esp32` core on every run. The local Arduino IDE
install used for F0 had core **3.3.8** (latest at the time: 3.3.10), so a sketch that
compiles locally may still behave differently in CI, and vice versa. Check with the
`arduino-cli` bundled in Arduino IDE:

```powershell
& "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" core list
```

## Integration tests

```powershell
pip install -r tests/integration/requirements.txt
docker compose -f gateway/docker-compose.yml up -d   # optional, enables broker tests
pytest tests/integration -v
```

Without a broker, `test_broker_roundtrip.py` is skipped. Point it elsewhere with
`$env:V2V_BROKER = "192.168.x.y:1883"`.
