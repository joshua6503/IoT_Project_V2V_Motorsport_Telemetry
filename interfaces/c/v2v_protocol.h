/**
 * @file v2v_protocol.h
 * @brief V2V wire protocol - single source of truth for the binary alert packet.
 *
 * Contract documented in interfaces/ble-gatt.md. Any change here MUST be
 * reflected there first (contracts-first, see docs/adr/0004).
 *
 * Portability: C99 + C++ (MCUXpresso/arm-none-eabi-gcc, Arduino-ESP32, host gcc).
 * Wire byte order: little-endian. Do NOT memcpy this struct onto the wire on a
 * big-endian host; serialize field by field.
 */
#ifndef V2V_PROTOCOL_H
#define V2V_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Protocol version                                                          */
/* ------------------------------------------------------------------------- */
#define V2V_PROTOCOL_VERSION_MAJOR 0u
#define V2V_PROTOCOL_VERSION_MINOR 1u
/** Encoded on the wire as one byte: high nibble = major, low nibble = minor. */
#define V2V_PROTOCOL_VERSION \
    ((uint8_t)((V2V_PROTOCOL_VERSION_MAJOR << 4) | (V2V_PROTOCOL_VERSION_MINOR & 0x0Fu)))

/* ------------------------------------------------------------------------- */
/* Portable compile-time assertion (C99 has no _Static_assert)               */
/* ------------------------------------------------------------------------- */
#if defined(__cplusplus) && (__cplusplus >= 201103L)
#define V2V_STATIC_ASSERT(cond, msg) static_assert(cond, #msg)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define V2V_STATIC_ASSERT(cond, msg) _Static_assert(cond, #msg)
#else
#define V2V_STATIC_ASSERT(cond, msg) typedef char v2v_static_assert_##msg[(cond) ? 1 : -1]
#endif

/* ------------------------------------------------------------------------- */
/* Enumerations (stored as uint8_t on the wire)                              */
/* Keep values in sync with the "alert_type"/"severity" enums of             */
/* interfaces/payload.schema.json (mapping table in interfaces/README.md).   */
/* ------------------------------------------------------------------------- */
typedef enum
{
    V2V_ALERT_NONE       = 0x00, /**< No alert (never transmitted). */
    V2V_ALERT_MANUAL     = 0x01, /**< Driver/test button (milestone M1). */
    V2V_ALERT_HARD_BRAKE = 0x02, /**< Hard braking detected (IMU). */
    V2V_ALERT_OBSTACLE   = 0x03, /**< Obstacle detected (distance sensor). */
    V2V_ALERT_OVERTEMP   = 0x04  /**< Over-temperature. */
} v2v_alert_type_t;

typedef enum
{
    V2V_SEVERITY_INFO     = 0x00,
    V2V_SEVERITY_WARNING  = 0x01,
    V2V_SEVERITY_CRITICAL = 0x02
} v2v_severity_t;

/** Bits of v2v_packet_t::flags. */
#define V2V_FLAG_CRC_PRESENT 0x01u /**< crc16 field is valid. */

/* ------------------------------------------------------------------------- */
/* Alert packet (16 bytes)                                                   */
/*                                                                           */
/*  off size field                                                           */
/*   0   1   version       V2V_PROTOCOL_VERSION                              */
/*   1   1   alert_type    v2v_alert_type_t                                  */
/*   2   1   severity      v2v_severity_t                                    */
/*   3   1   flags         V2V_FLAG_*                                        */
/*   4   2   node_id       sender id (see interfaces/README.md)              */
/*   6   2   seq           per-sender counter, wraps at 65535                */
/*   8   4   timestamp_ms  sender uptime in ms (time base TBD)               */
/*  12   2   crc16         CRC-16/CCITT-FALSE over bytes 0..11 (optional)    */
/*  14   2   reserved      must be 0                                         */
/*                                                                           */
/* Fields are ordered so the struct is naturally aligned (no padding on any  */
/* common ABI); pack(1) is kept as a belt-and-braces measure.                */
/* ------------------------------------------------------------------------- */
#if defined(__GNUC__) || defined(_MSC_VER)
#pragma pack(push, 1)
#endif
typedef struct
{
    uint8_t version;
    uint8_t alert_type;
    uint8_t severity;
    uint8_t flags;
    uint16_t node_id;
    uint16_t seq;
    uint32_t timestamp_ms;
    uint16_t crc16;
    uint16_t reserved;
} v2v_packet_t;
#if defined(__GNUC__) || defined(_MSC_VER)
#pragma pack(pop)
#endif

#define V2V_PACKET_SIZE     16u
#define V2V_CRC_COVERED_LEN 12u /**< Bytes 0..11 are covered by crc16. */

V2V_STATIC_ASSERT(sizeof(v2v_packet_t) == V2V_PACKET_SIZE, v2v_packet_size);

/* ------------------------------------------------------------------------- */
/* NodeInfo characteristic payload (8 bytes, read-only)                      */
/* ------------------------------------------------------------------------- */
/** Bits of v2v_node_info_t::capabilities (actuators the node can drive). */
#define V2V_CAP_LED    0x01u
#define V2V_CAP_BUZZER 0x02u
#define V2V_CAP_HAPTIC 0x04u
#define V2V_CAP_RELAY  0x08u

#if defined(__GNUC__) || defined(_MSC_VER)
#pragma pack(push, 1)
#endif
typedef struct
{
    uint8_t version;      /**< V2V_PROTOCOL_VERSION */
    uint8_t capabilities; /**< V2V_CAP_* bitmask */
    uint16_t node_id;
    uint8_t fw_major;
    uint8_t fw_minor;
    uint8_t fw_patch;
    uint8_t reserved;     /**< must be 0 */
} v2v_node_info_t;
#if defined(__GNUC__) || defined(_MSC_VER)
#pragma pack(pop)
#endif

#define V2V_NODE_INFO_SIZE 8u

V2V_STATIC_ASSERT(sizeof(v2v_node_info_t) == V2V_NODE_INFO_SIZE, v2v_node_info_size);

#ifdef __cplusplus
}
#endif

#endif /* V2V_PROTOCOL_H */
