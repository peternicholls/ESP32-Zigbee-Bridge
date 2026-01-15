/**
 * @file os_persist.c
 * @brief Persistence service implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - NVS wrapper
 * 
 * On host: Uses file-based storage for testing.
 * On ESP32: Uses ESP-IDF NVS.
 */

#include "os_persist.h"
#include "os_fibre.h"
#include "os_log.h"
#include "os_event.h"
#include "os_config.h"
#include <string.h>
#include <stdio.h>

#define PERSIST_MODULE "PERSIST"

#ifdef OS_PLATFORM_HOST
/* Host implementation using simple file storage */
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

#define PERSIST_DIR "/tmp/bridge_persist"
#define SCHEMA_KEY "_schema_version"
#define PATH_MAX_LEN 128
/* Maximum filename length within path buffer (accounts for PERSIST_DIR + "/" + ".bin") */
#define FILENAME_MAX_LEN (PATH_MAX_LEN - sizeof(PERSIST_DIR) - 6)

/* Buffered write entry */
typedef struct {
    char key[OS_PERSIST_KEY_MAX];
    uint8_t data[OS_PERSIST_VALUE_MAX];
    size_t len;
    bool valid;
} write_buffer_entry_t;

#define WRITE_BUFFER_SIZE 16

static struct {
    bool initialized;
    write_buffer_entry_t write_buffer[WRITE_BUFFER_SIZE];
    uint32_t writes_buffered;
    uint32_t total_writes;
    uint32_t total_reads;
    uint32_t schema_version;
} persist = {0};

/* Create storage directory */
static os_err_t ensure_dir(void) {
    struct stat st;
    if (stat(PERSIST_DIR, &st) == 0) {
        return OS_OK;
    }
    if (mkdir(PERSIST_DIR, 0755) != 0) {
        LOG_E(PERSIST_MODULE, "Failed to create persist dir: %s", strerror(errno));
        return OS_ERR_BUSY;
    }
    return OS_OK;
}

/* Generate file path for key */
static void key_to_path(const char *key, char *path, size_t path_len) {
    snprintf(path, path_len, "%s/%.32s.bin", PERSIST_DIR, key);
}

/* Write data to file */
static os_err_t write_file(const char *key, const void *data, size_t len) {
    char path[PATH_MAX_LEN];
    key_to_path(key, path, sizeof(path));
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        LOG_E(PERSIST_MODULE, "Failed to open %s for write", path);
        return OS_ERR_BUSY;
    }
    
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    
    if (written != len) {
        LOG_E(PERSIST_MODULE, "Write incomplete: %zu/%zu", written, len);
        return OS_ERR_BUSY;
    }
    
    return OS_OK;
}

/* Read data from file */
static os_err_t read_file(const char *key, void *buf, size_t buf_len, size_t *out_len) {
    char path[PATH_MAX_LEN];
    key_to_path(key, path, sizeof(path));
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        return OS_ERR_NOT_FOUND;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size < 0 || (size_t)size > buf_len) {
        fclose(f);
        return OS_ERR_NO_MEM;
    }
    
    size_t read_len = fread(buf, 1, (size_t)size, f);
    fclose(f);
    
    if (out_len) {
        *out_len = read_len;
    }
    
    return OS_OK;
}

/* Delete file */
static os_err_t delete_file(const char *key) {
    char path[PATH_MAX_LEN];
    key_to_path(key, path, sizeof(path));
    
    if (unlink(path) != 0 && errno != ENOENT) {
        return OS_ERR_BUSY;
    }
    
    return OS_OK;
}

os_err_t os_persist_init(void) {
    if (persist.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }
    
    memset(&persist, 0, sizeof(persist));
    
    os_err_t err = ensure_dir();
    if (err != OS_OK) {
        return err;
    }
    
    /* Load schema version if exists */
    uint32_t version = 0;
    size_t len;
    if (read_file(SCHEMA_KEY, &version, sizeof(version), &len) == OS_OK) {
        persist.schema_version = version;
    }
    
    persist.initialized = true;
    LOG_I(PERSIST_MODULE, "Persistence initialized (schema v%u)", persist.schema_version);
    
    return OS_OK;
}

os_err_t os_persist_put(const char *key, const void *data, size_t len) {
    if (!persist.initialized || !key || !data) {
        return OS_ERR_INVALID_ARG;
    }
    
    if (len > OS_PERSIST_VALUE_MAX) {
        return OS_ERR_INVALID_ARG;
    }
    
    /* Find or create buffer slot */
    write_buffer_entry_t *slot = NULL;
    
    /* First, look for existing entry with same key */
    for (int i = 0; i < WRITE_BUFFER_SIZE; i++) {
        if (persist.write_buffer[i].valid && 
            strcmp(persist.write_buffer[i].key, key) == 0) {
            slot = &persist.write_buffer[i];
            break;
        }
    }
    
    /* If not found, find empty slot */
    if (!slot) {
        for (int i = 0; i < WRITE_BUFFER_SIZE; i++) {
            if (!persist.write_buffer[i].valid) {
                slot = &persist.write_buffer[i];
                persist.writes_buffered++;
                break;
            }
        }
    }
    
    if (!slot) {
        /* Buffer full, flush first */
        os_persist_flush();
        slot = &persist.write_buffer[0];
        persist.writes_buffered++;
    }
    
    /* Copy to buffer */
    strncpy(slot->key, key, OS_PERSIST_KEY_MAX - 1);
    slot->key[OS_PERSIST_KEY_MAX - 1] = '\0';
    memcpy(slot->data, data, len);
    slot->len = len;
    slot->valid = true;
    
    LOG_T(PERSIST_MODULE, "Buffered write: %s (%zu bytes)", key, len);
    
    return OS_OK;
}

os_err_t os_persist_get(const char *key, void *buf, size_t buf_len, size_t *out_len) {
    if (!persist.initialized || !key || !buf) {
        return OS_ERR_INVALID_ARG;
    }
    
    persist.total_reads++;
    
    /* Check write buffer first */
    for (int i = 0; i < WRITE_BUFFER_SIZE; i++) {
        if (persist.write_buffer[i].valid && 
            strcmp(persist.write_buffer[i].key, key) == 0) {
            size_t copy_len = persist.write_buffer[i].len;
            if (copy_len > buf_len) {
                copy_len = buf_len;
            }
            memcpy(buf, persist.write_buffer[i].data, copy_len);
            if (out_len) {
                *out_len = persist.write_buffer[i].len;
            }
            return OS_OK;
        }
    }
    
    /* Read from storage */
    return read_file(key, buf, buf_len, out_len);
}

os_err_t os_persist_del(const char *key) {
    if (!persist.initialized || !key) {
        return OS_ERR_INVALID_ARG;
    }
    
    /* Remove from buffer if present */
    for (int i = 0; i < WRITE_BUFFER_SIZE; i++) {
        if (persist.write_buffer[i].valid && 
            strcmp(persist.write_buffer[i].key, key) == 0) {
            persist.write_buffer[i].valid = false;
            persist.writes_buffered--;
        }
    }
    
    /* Delete from storage */
    return delete_file(key);
}

bool os_persist_exists(const char *key) {
    if (!persist.initialized || !key) {
        return false;
    }
    
    /* Check buffer */
    for (int i = 0; i < WRITE_BUFFER_SIZE; i++) {
        if (persist.write_buffer[i].valid && 
            strcmp(persist.write_buffer[i].key, key) == 0) {
            return true;
        }
    }
    
    /* Check storage */
    char path[PATH_MAX_LEN];
    key_to_path(key, path, sizeof(path));
    
    struct stat st;
    return stat(path, &st) == 0;
}

os_err_t os_persist_flush(void) {
    if (!persist.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    uint32_t flushed = 0;
    
    for (int i = 0; i < WRITE_BUFFER_SIZE; i++) {
        if (persist.write_buffer[i].valid) {
            os_err_t err = write_file(
                persist.write_buffer[i].key,
                persist.write_buffer[i].data,
                persist.write_buffer[i].len
            );
            
            if (err == OS_OK) {
                persist.write_buffer[i].valid = false;
                persist.total_writes++;
                flushed++;
            } else {
                LOG_E(PERSIST_MODULE, "Failed to flush %s", persist.write_buffer[i].key);
            }
        }
    }
    
    persist.writes_buffered = 0;
    
    if (flushed > 0) {
        LOG_D(PERSIST_MODULE, "Flushed %u writes", flushed);
        os_event_emit(OS_EVENT_PERSIST_FLUSH, &flushed, sizeof(flushed));
    }
    
    return OS_OK;
}

uint32_t os_persist_schema_version(void) {
    return persist.schema_version;
}

os_err_t os_persist_set_schema_version(uint32_t version) {
    persist.schema_version = version;
    return os_persist_put(SCHEMA_KEY, &version, sizeof(version));
}

os_err_t os_persist_erase_all(void) {
    if (!persist.initialized) {
        return OS_ERR_NOT_INITIALIZED;
    }
    
    /* Clear buffer */
    memset(persist.write_buffer, 0, sizeof(persist.write_buffer));
    persist.writes_buffered = 0;
    
    /* Remove all files in directory */
    DIR *dir = opendir(PERSIST_DIR);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] != '.') {
                char path[PATH_MAX_LEN];
                /* Limit filename to prevent buffer overflow */
                int written = snprintf(path, sizeof(path), "%s/%.*s", 
                                       PERSIST_DIR, (int)FILENAME_MAX_LEN, entry->d_name);
                if (written > 0 && (size_t)written < sizeof(path)) {
                    unlink(path);
                }
            }
        }
        closedir(dir);
    }
    
    persist.schema_version = 0;
    LOG_I(PERSIST_MODULE, "Storage erased");
    
    return OS_OK;
}

void os_persist_get_stats(uint32_t *writes_buffered, uint32_t *total_writes, uint32_t *total_reads) {
    if (writes_buffered) *writes_buffered = persist.writes_buffered;
    if (total_writes) *total_writes = persist.total_writes;
    if (total_reads) *total_reads = persist.total_reads;
}

#else
/* ESP32 NVS implementation would go here */
#error "ESP32 NVS implementation not yet available"
#endif

void os_persist_task(void *arg) {
    (void)arg;
    
    LOG_I(PERSIST_MODULE, "Persistence task started");
    
    while (1) {
        os_sleep(OS_PERSIST_FLUSH_MS);
        
        if (persist.writes_buffered > 0) {
            os_persist_flush();
        }
    }
}
