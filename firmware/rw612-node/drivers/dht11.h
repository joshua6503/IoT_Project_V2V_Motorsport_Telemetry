/**
 * @file dht11.h
 * @brief DHT11 temperature/humidity sensor, bit-banged on one GPIO line.
 *
 * Non-blocking start: dht11_start() pulls the line low and returns; dht11_poll(),
 * called every loop iteration, releases it once the start time has elapsed and
 * then reads the 40-bit frame (blocks ~5 ms, never the 18 ms start pulse).
 */
#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>

/** Minimum time between reads: "sampling period > 2 seconds" [DHT11 datasheet, Aosong, p.3]. */
#define DHT11_MIN_INTERVAL_MS 2000u

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DHT11_OK = 0,          /**< new reading in *temp_c */
    DHT11_BUSY,            /**< nothing to report yet (idle or start pulse running) */
    DHT11_ERR_TOO_SOON,    /**< dht11_start() before the minimum interval */
    DHT11_ERR_NO_RESPONSE, /**< sensor did not answer the start signal */
    DHT11_ERR_TIMEOUT,     /**< frame stopped mid-bit */
    DHT11_ERR_CHECKSUM     /**< frame received but checksum mismatch */
} dht11_status_t;

/** Release the line (input, idle high via pull-up). */
void dht11_init(void);

/**
 * @brief Begin a measurement (drive the start pulse).
 * @return DHT11_BUSY if started (or one is already running), DHT11_ERR_TOO_SOON otherwise.
 */
dht11_status_t dht11_start(uint32_t now_ms);

/**
 * @brief Advance the measurement. Call every loop iteration.
 * @param[out] temp_c Written only when DHT11_OK is returned.
 * @return DHT11_OK / an error once per started measurement, DHT11_BUSY otherwise.
 */
dht11_status_t dht11_poll(uint32_t now_ms, float *temp_c);

#ifdef __cplusplus
}
#endif

#endif /* DHT11_H */
