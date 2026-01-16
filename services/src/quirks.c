/**
 * @file quirks.c
 * @brief Device quirks table implementation
 * 
 * ESP32-C6 Zigbee Bridge OS - Device quirks system
 * 
 * Pre-defined table of quirks for non-standard Zigbee devices.
 */

#include "quirks.h"
#include "os.h"
#include <string.h>
#include <math.h>

#define QUIRKS_MODULE "QUIRKS"

/* Epsilon for floating point comparisons */
#define QUIRKS_FLOAT_EPSILON 1e-6f

/* Built-in quirks table */
static const quirk_entry_t quirks_table[] = {
    /* Example: DUMMY test device with level clamping */
    {
        .manufacturer = "DUMMY",
        .model = "DUMMY-LIGHT-1",
        .prefix_match = false,
        .actions = {
            {
                .type = QUIRK_ACTION_CLAMP_RANGE,
                .target_cap = CAP_LIGHT_LEVEL,
                .params.clamp = { .min = 1, .max = 100 }
            }
        },
        .action_count = 1
    },
    
    /* IKEA TRADFRI bulbs often need level minimum clamping */
    {
        .manufacturer = "IKEA of Sweden",
        .model = "TRADFRI bulb",
        .prefix_match = true,
        .actions = {
            {
                .type = QUIRK_ACTION_CLAMP_RANGE,
                .target_cap = CAP_LIGHT_LEVEL,
                .params.clamp = { .min = 1, .max = 100 }
            }
        },
        .action_count = 1
    },
    
    /* Xiaomi/Aqara sensors that report inverted contact state */
    {
        .manufacturer = "LUMI",
        .model = "lumi.sensor_magnet",
        .prefix_match = true,
        .actions = {
            {
                .type = QUIRK_ACTION_INVERT_BOOLEAN,
                .target_cap = CAP_SENSOR_CONTACT,
                .params.invert = { .enabled = true }
            }
        },
        .action_count = 1
    },
    
    /* Tuya devices with scaled temperature */
    {
        .manufacturer = "_TZE200",
        .model = "TS0601",
        .prefix_match = true,
        .actions = {
            {
                .type = QUIRK_ACTION_SCALE_NUMERIC,
                .target_cap = CAP_SENSOR_TEMPERATURE,
                .params.scale = { .multiplier = 0.1f, .offset = 0.0f }
            }
        },
        .action_count = 1
    }
};

#define QUIRKS_TABLE_SIZE (sizeof(quirks_table) / sizeof(quirks_table[0]))

/* Action type names */
static const char *action_names[] = {
    "none",
    "clamp_range",
    "invert_boolean",
    "scale_numeric",
    "remap_attribute",
    "override_reporting",
    "ignore_spurious"
};

/* Service state */
static struct {
    bool initialized;
} service = {0};

os_err_t quirks_init(void) {
    if (service.initialized) {
        return OS_ERR_ALREADY_EXISTS;
    }
    
    service.initialized = true;
    
    LOG_I(QUIRKS_MODULE, "Quirks service initialized (%zu entries)", QUIRKS_TABLE_SIZE);
    
    return OS_OK;
}

const quirk_entry_t *quirks_find(const char *manufacturer, const char *model) {
    if (!manufacturer || !model) {
        return NULL;
    }
    
    for (size_t i = 0; i < QUIRKS_TABLE_SIZE; i++) {
        const quirk_entry_t *entry = &quirks_table[i];
        
        /* Check manufacturer match */
        if (strcmp(entry->manufacturer, manufacturer) != 0) {
            continue;
        }
        
        /* Check model match (exact or prefix) */
        if (entry->prefix_match) {
            if (strncmp(model, entry->model, strlen(entry->model)) == 0) {
                return entry;
            }
        } else {
            if (strcmp(model, entry->model) == 0) {
                return entry;
            }
        }
    }
    
    return NULL;
}

os_err_t quirks_apply_value(const char *manufacturer, const char *model,
                             cap_id_t cap_id, cap_value_t *value,
                             quirk_result_t *result) {
    if (!value) {
        return OS_ERR_INVALID_ARG;
    }
    
    if (result) {
        result->applied = false;
        result->actions_applied = 0;
    }
    
    const quirk_entry_t *entry = quirks_find(manufacturer, model);
    if (!entry) {
        return OS_OK;  /* No quirks, value unchanged */
    }
    
    /* Validate action count against maximum to avoid out-of-bounds access */
    if (entry->action_count > QUIRK_MAX_ACTIONS) {
        return OS_ERR_INVALID_ARG;
    }
    
    /* Apply matching actions */
    for (uint8_t i = 0; i < entry->action_count; i++) {
        const quirk_action_t *action = &entry->actions[i];
        
        if (action->target_cap != cap_id) {
            continue;
        }
        
        switch (action->type) {
            case QUIRK_ACTION_CLAMP_RANGE:
                /* Clamp integer value */
                if (value->i < action->params.clamp.min) {
                    value->i = action->params.clamp.min;
                } else if (value->i > action->params.clamp.max) {
                    value->i = action->params.clamp.max;
                }
                LOG_D(QUIRKS_MODULE, "Applied clamp_range to cap %d", cap_id);
                break;
                
            case QUIRK_ACTION_INVERT_BOOLEAN:
                if (action->params.invert.enabled) {
                    value->b = !value->b;
                    LOG_D(QUIRKS_MODULE, "Applied invert_boolean to cap %d", cap_id);
                }
                break;
                
            case QUIRK_ACTION_SCALE_NUMERIC:
                value->f = value->f * action->params.scale.multiplier + action->params.scale.offset;
                LOG_D(QUIRKS_MODULE, "Applied scale_numeric to cap %d", cap_id);
                break;
                
            default:
                continue;
        }
        
        if (result) {
            result->applied = true;
            result->actions_applied++;
        }
    }
    
    return OS_OK;
}

os_err_t quirks_apply_command(const char *manufacturer, const char *model,
                               cap_id_t cap_id, cap_value_t *value,
                               quirk_result_t *result) {
    if (!value) {
        return OS_ERR_INVALID_ARG;
    }
    
    if (result) {
        result->applied = false;
        result->actions_applied = 0;
    }
    
    const quirk_entry_t *entry = quirks_find(manufacturer, model);
    if (!entry) {
        return OS_OK;  /* No quirks, value unchanged */
    }
    
    /* Validate action count against maximum to avoid out-of-bounds access */
    if (entry->action_count > QUIRK_MAX_ACTIONS) {
        return OS_ERR_INVALID_ARG;
    }
    
    /* Apply inverse of matching actions for commands */
    for (uint8_t i = 0; i < entry->action_count; i++) {
        const quirk_action_t *action = &entry->actions[i];
        
        if (action->target_cap != cap_id) {
            continue;
        }
        
        switch (action->type) {
            case QUIRK_ACTION_CLAMP_RANGE:
                /* Clamp still applies for commands */
                if (value->i < action->params.clamp.min) {
                    value->i = action->params.clamp.min;
                } else if (value->i > action->params.clamp.max) {
                    value->i = action->params.clamp.max;
                }
                break;
                
            case QUIRK_ACTION_INVERT_BOOLEAN:
                /* Invert on command as well */
                if (action->params.invert.enabled) {
                    value->b = !value->b;
                }
                break;
                
            case QUIRK_ACTION_SCALE_NUMERIC:
                /* Reverse scale for commands */
                if (fabsf(action->params.scale.multiplier) > QUIRKS_FLOAT_EPSILON) {
                    value->f = (value->f - action->params.scale.offset) / action->params.scale.multiplier;
                }
                break;
                
            default:
                continue;
        }
        
        if (result) {
            result->applied = true;
            result->actions_applied++;
        }
    }
    
    return OS_OK;
}

uint32_t quirks_count(void) {
    return QUIRKS_TABLE_SIZE;
}

const quirk_entry_t *quirks_get_entry(uint32_t index) {
    if (index >= QUIRKS_TABLE_SIZE) {
        return NULL;
    }
    return &quirks_table[index];
}

const char *quirks_action_name(quirk_action_type_t type) {
    if (type < sizeof(action_names) / sizeof(action_names[0])) {
        return action_names[type];
    }
    return "unknown";
}
