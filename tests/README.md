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
Options: MSYS2/MinGW-w64 `gcc`, or run in WSL. The Makefile clones Unity
(pinned tag) into `tests/unit/vendor/` on first run (git-ignored).

```powershell
make -C tests/unit            # or: make -C tests/unit CC=clang
```

## Integration tests

```powershell
pip install -r tests/integration/requirements.txt
docker compose -f gateway/docker-compose.yml up -d   # optional, enables broker tests
pytest tests/integration -v
```

Without a broker, `test_broker_roundtrip.py` is skipped. Point it elsewhere with
`$env:V2V_BROKER = "192.168.x.y:1883"`.
