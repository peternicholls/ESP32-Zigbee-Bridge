/**
 * @file os_persist.h
 * @brief Persistence service API
 * 
 * ESP32-C6 Zigbee Bridge OS - NVS wrapper for persistent storage
 * 
 * Features:
 * - Key-value blob storage
 * - Schema versioning
 * - Buffered writes with periodic flush
 */

#ifndef OS_PERSIST_H
#define OS_PERSIST_H

#include "os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum key length */
#define OS_PERSIST_KEY_MAX  32

/* Maximum value size */
#define OS_PERSIST_VALUE_MAX  512

/**
 * @brief Initialize persistence service
 * @return OS_OK on success
 */
os_err_t os_persist_init(void);

/**
 * @brief Store a blob value
 * @param key Key string
 * @param data Data buffer
 * @param len Data length
 * @return OS_OK on success
 */
os_err_t os_persist_put(const char *key, const void *data, size_t len);

/**
 * @brief Retrieve a blob value
 * @param key Key string
 * @param buf Output buffer
 * @param buf_len Buffer size
 * @param out_len Actual data length (can be NULL)
 * @return OS_OK on success, OS_ERR_NOT_FOUND if key doesn't exist
 */
os_err_t os_persist_get(const char *key, void *buf, size_t buf_len, size_t *out_len);

/**
 * @brief Delete a key
 * @param key Key string
 * @return OS_OK on success
 */
os_err_t os_persist_del(const char *key);

/**
 * @brief Check if a key exists
 * @param key Key string
 * @return true if key exists
 */
bool os_persist_exists(const char *key);

/**
 * @brief Flush buffered writes to storage
 * @return OS_OK on success
 */
os_err_t os_persist_flush(void);

/**
 * @brief Get current schema version
 * @return Schema version number
 */
uint32_t os_persist_schema_version(void);

/**
 * @brief Set schema version
 * @param version New version
 * @return OS_OK on success
 */
os_err_t os_persist_set_schema_version(uint32_t version);

/**
 * @brief Erase all persisted data
 * @return OS_OK on success
 */
os_err_t os_persist_erase_all(void);

/**
 * @brief Get persistence statistics
 * @param writes_buffered Output: buffered writes pending
 * @param total_writes Output: total write operations
 * @param total_reads Output: total read operations
 */
void os_persist_get_stats(uint32_t *writes_buffered, uint32_t *total_writes, uint32_t *total_reads);

typedef struct {
    uint32_t writes_buffered;
    uint32_t total_writes;
    uint32_t total_reads;
    os_tick_t last_flush_tick;
    os_err_t last_error;
} os_persist_stats_t;

/**
 * @brief Get extended persistence statistics
 * @param stats Output stats structure
 */
void os_persist_get_stats_ex(os_persist_stats_t *stats);

/* Persistence task for periodic flush (run as fibre) */
void os_persist_task(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* OS_PERSIST_H */
