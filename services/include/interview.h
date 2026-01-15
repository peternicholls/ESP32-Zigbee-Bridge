/**
 * @file interview.h
 * @brief Interview/provisioner service API
 * 
 * ESP32-C6 Zigbee Bridge OS - Device interview service
 * 
 * Handles device discovery:
 * - Query endpoints (simple descriptor)
 * - Query clusters per endpoint
 * - Read Basic cluster attributes (manufacturer/model/SW build)
 * - Persist and resume interview progress
 */

#ifndef INTERVIEW_H
#define INTERVIEW_H

#include "os_types.h"
#include "reg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Interview stages */
typedef enum {
    INTERVIEW_STAGE_INIT = 0,
    INTERVIEW_STAGE_ACTIVE_EP,      /* Getting active endpoints */
    INTERVIEW_STAGE_SIMPLE_DESC,    /* Getting simple descriptors */
    INTERVIEW_STAGE_BASIC_ATTR,     /* Reading basic cluster attributes */
    INTERVIEW_STAGE_BINDINGS,       /* Setting up bindings */
    INTERVIEW_STAGE_COMPLETE,       /* Interview complete */
    INTERVIEW_STAGE_FAILED,         /* Interview failed */
} interview_stage_t;

/**
 * @brief Initialize interview service
 * @return OS_OK on success
 */
os_err_t interview_init(void);

/**
 * @brief Start interview for a node
 * @param ieee_addr Node IEEE address
 * @return OS_OK on success
 */
os_err_t interview_start(os_eui64_t ieee_addr);

/**
 * @brief Continue interview process (call from fibre)
 * This advances any pending interviews
 */
void interview_process(void);

/**
 * @brief Get interview stage for a node
 * @param ieee_addr Node IEEE address
 * @return Current interview stage
 */
interview_stage_t interview_get_stage(os_eui64_t ieee_addr);

/**
 * @brief Cancel interview for a node
 * @param ieee_addr Node IEEE address
 * @return OS_OK on success
 */
os_err_t interview_cancel(os_eui64_t ieee_addr);

/**
 * @brief Interview task entry (run as fibre)
 * @param arg Unused
 */
void interview_task(void *arg);

/**
 * @brief Get stage name string
 * @param stage Stage value
 * @return Stage name
 */
const char *interview_stage_name(interview_stage_t stage);

#ifdef __cplusplus
}
#endif

#endif /* INTERVIEW_H */
