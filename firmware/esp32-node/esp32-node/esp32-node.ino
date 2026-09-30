/**
 * esp32-node.ino - secondary V2V node (ESP32 classic, Arduino core).
 *
 * Scaffold only: prints the protocol version and does nothing else.
 * BLE central (M1) and MQTT client (M2) come later, see interfaces/.
 */
#include "config.h"
#include "src/v2v_protocol.h"

void setup()
{
    Serial.begin(115200);
    Serial.printf("[%s] v2v protocol v%u.%u, packet %u bytes\n",
                  NODE_ID_STR,
                  (unsigned)V2V_PROTOCOL_VERSION_MAJOR,
                  (unsigned)V2V_PROTOCOL_VERSION_MINOR,
                  (unsigned)sizeof(v2v_packet_t));
}

void loop()
{
    delay(1000);
}
