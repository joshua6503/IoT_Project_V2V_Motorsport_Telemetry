# services/

Protocol and behaviour services, one module per prefix:

- `v2v_` — BLE GATT V2V service (`interfaces/ble-gatt.md`).
- `tlm_` — telemetry over MQTT (`interfaces/mqtt-topics.md`): `tlm_mqtt.{c,h}` joins Wi-Fi (SDK WPL), connects to the
  broker with a retained `online` status and an `offline` LWT, reconnects, publishes telemetry (QoS 0) and alerts (QoS 1).
  JSON comes from `firmware/common/tlm_json`. Uses lwIP/WPL headers, no `fsl_*` includes; credentials are passed in by
  the app (`tlm_mqtt_config_t`), never read from a header here.
- `alert_` — actuation patterns for received alerts.

Uses `drivers/` and `firmware/common/`. No direct SDK peripheral calls.
