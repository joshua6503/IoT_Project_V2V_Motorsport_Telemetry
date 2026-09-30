## What

<!-- One or two sentences. Link the issue: Closes #N -->

## Why

## How it was tested

- [ ] `make -C tests/unit`
- [ ] `pytest tests/integration`
- [ ] On hardware (describe: board, firmware, what was observed)

## Checklist

- [ ] Protocol changes were made in `interfaces/` first (and `tools/sync_protocol.py` was run)
- [ ] New decision → ADR added in `docs/adr/`
- [ ] New pure logic in `firmware/common/` has a unit test
- [ ] No invented pins/specs (unknowns left as `TBD`)
- [ ] No NXP SDK files, no secrets (`config.h`) committed
- [ ] Commit messages follow Conventional Commits
