# services/

Protocol and behaviour services, one module per prefix:

- `v2v_` — BLE GATT V2V service (`interfaces/ble-gatt.md`).
- `tlm_` — telemetry over MQTT (`interfaces/mqtt-topics.md`).
- `alert_` — actuation patterns for received alerts.

Uses `drivers/` and `firmware/common/`. No direct SDK peripheral calls.
