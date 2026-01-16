/**
 * @file local_node.h
 * @brief Local hardware node service
 */

#ifndef LOCAL_NODE_H
#define LOCAL_NODE_H

#include "os_types.h"

os_err_t local_node_init(void);
void local_node_task(void *arg);

#endif /* LOCAL_NODE_H */
