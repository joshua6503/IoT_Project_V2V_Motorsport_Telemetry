# services/

Protocol and behaviour services, one module per prefix:

- `v2v_` — BLE GATT V2V service (`interfaces/ble-gatt.md`).
- `tlm_` — telemetry over MQTT (`interfaces/mqtt-topics.md`): `tlm_mqtt.{c,h}` joins Wi-Fi (SDK WPL), connects to the
  broker with a retained `online` status and an `offline` LWT, reconnects, publishes telemetry (QoS 0) and alerts (QoS 1).
  JSON comes from `firmware/common/tlm_json`. Uses lwIP/WPL headers, no `fsl_*` includes; credentials are passed in by
  the app (`tlm_mqtt_config_t`), never read from a header here.
  It also subscribes to the DEC-013 command topics (`v2v/{node_id}/cmd`, `v2v/all/cmd`, QoS 1) on every connect and hands
  parsed commands (`firmware/common/alert_cmd`) to the app through a queue (`tlm_mqtt_cmd_poll`).
- `alert_` — `alert_actuation.{c,h}` applies the DEC-004 patterns to the LEDs/buzzer and merges a remote alert
  from a dashboard command (`alert_actuation_remote`) with the local one.

Uses `drivers/` and `firmware/common/`. No direct SDK peripheral calls.
