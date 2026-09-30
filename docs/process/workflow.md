# Workflow

How the project moves from an idea to a committed change. Live state is tracked in
[`../STATUS.md`](../STATUS.md).

## Roles

| Role | Who | Does | Does not |
|---|---|---|---|
| Architect / PM | claude.ai Project chats | debates, proposes, audits, keeps STATUS, writes handoffs | write to the repo |
| Implementer | Claude Code in this repo | applies approved handoffs, runs tests/builds, ends with a commit block | take new decisions; it reports CONFLICTs |
| Technical advisor | `rw612-tutor` (separate folder) | answers SDK/peripheral questions with citations | decide architecture |
| Approver | Joshua / the team | moves items to DECIDED | — |

**Cycle:** chat (debate → approval) → handoff → Claude Code (write + verify) →
commit block → manual push → "Sync now" in the claude.ai Project.

## Statuses

`PROPOSED`, `DECIDED`, `OPEN`, `ASSUMPTION` (to validate), `CONFLICT`, `REJECTED`.

- Only the approver moves an item to `DECIDED`.
- A hardware or SDK fact needs a citation; without one it is an `ASSUMPTION`.

## IDs

Keep the existing IDs (`REQ-F-xxx` / `REQ-NF-xxx`, `ADR-NNNN`, `HIL-NN`). New ones:

| Prefix | Use |
|---|---|
| `DEC-###` | decisions without an ADR |
| `OPEN-###` | open questions |
| `CONFLICT-###` | contradictions |

They live in [`docs/STATUS.md`](../STATUS.md).

## Layers and gates

A layer closes when all of its items are `DECIDED` or explicitly `TBD` with an owner.
Code for milestone Mx starts only when L0–L3 are closed and the L4 items that Mx
needs are `DECIDED`.

| Layer | Content | Location |
|---|---|---|
| L0 Vision & scope | problem, users, in/out of scope | [`docs/00-vision/`](../00-vision/) |
| L1 Requirements | REQ-F / REQ-NF | [`docs/01-requirements/`](../01-requirements/) |
| L2 Architecture | system planes, 4-layer view, firmware layers, ADRs | [`docs/02-architecture/`](../02-architecture/), [`docs/adr/`](../adr/) |
| L3 Contracts | MQTT, JSON Schema, BLE GATT, C header | [`interfaces/`](../../interfaces/) |
| L4 Hardware | BOM, pinout (cited sources) | [`hardware/`](../../hardware/) |
| L5 Implementation | firmware, gateway, dashboard, tools — per milestone | [`firmware/`](../../firmware/), [`gateway/`](../../gateway/), [`dashboard/`](../../dashboard/), [`tools/`](../../tools/) |
| L6 Verification | unit, integration, HIL, evidence | [`tests/`](../../tests/), [`docs/04-testing/`](../04-testing/) |
| L7 Deliverables | school documents (Spanish) | [`docs/deliverables/`](../deliverables/) |

## ADR policy

An ADR is only for consequential decisions: radio or transport, wire protocol,
BLE roles, toolchain, firmware architecture, data ownership.

Minor decisions become a `DEC-###` entry in STATUS plus a dated line in
[`docs/project-notes.md`](../project-notes.md).

## Handoff format

Written by the chat, executed by Claude Code.

```markdown
# HANDOFF-NNN — <title>
Role: implementer, no new decisions; report CONFLICTs.
Objective: <one paragraph>
Approved items: <IDs moving to DECIDED>
Changes: <file-by-file list>
Acceptance criteria: <checks, tests to run>
Out of scope: <what must not be touched>
Closing: commit block
```

## Commit block

Claude Code closes every session with it:

```powershell
Set-Location "C:\Users\joshu\Desktop\IOT\v2v-motorsport-telemetry"
git add <exact files touched in this session>
git commit -m "<conventional commit naming the IDs closed>"
git push
```

Followed by the line: *After the push: "Sync now" in the claude.ai Project.*

Rules:

- List explicit paths; never `git add -A`.
- If a session covers several topics, use one block per topic.
- Never include ignored files: `config.h`, `.venv/`, SDK files.
- Write the files first; the block goes last.
- Do not explain that you cannot push.
