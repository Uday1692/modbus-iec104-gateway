/**
 * @file iec104_server.h
 * @brief IEC 104 Server Interface
 * 
 * This module provides functionality to serve data via IEC 104 protocol.
 * For now, this is a basic interface that will be implemented with libasdu.
 */

#ifndef IEC104_SERVER_H
#define IEC104_SERVER_H

#include <stdint.h>
#include <time.h>

typedef struct {
    char *bind_address;
    int port;
    int max_clients;
} iec104_config_t;

typedef struct iec104_server {
    int listen_socket;
    iec104_config_t config;
    int running;
    void *data_points;  /* List of data points */
} iec104_server_t;

typedef enum {
    IEC104_DT_BOOLEAN,
    IEC104_DT_INT32,
    IEC104_DT_UINT16,
    IEC104_DT_FLOAT,
} iec104_data_type_t;

typedef struct {
    uint32_t ioa;  /* Information Object Address */
    iec104_data_type_t type;
    union {
        uint8_t boolean_val;
        int32_t int32_val;
        uint16_t uint16_val;
        float float_val;
    } value;
    time_t timestamp;
    uint8_t quality;
} iec104_data_point_t;

/**
 * Initialize IEC 104 server
 * @param config Configuration structure
 * @return Pointer to iec104_server_t or NULL on error
 */
iec104_server_t *iec104_server_init(iec104_config_t *config);

/**
 * Start IEC 104 server (non-blocking)
 * @param server IEC 104 server handle
 * @return 0 on success, -1 on error
 */
int iec104_server_start(iec104_server_t *server);

/**
 * Stop IEC 104 server
 * @param server IEC 104 server handle
 * @return 0 on success, -1 on error
 */
int iec104_server_stop(iec104_server_t *server);

/**
 * Register a data point in the IEC 104 server
 * @param server IEC 104 server handle
 * @param point Data point to register
 * @return 0 on success, -1 on error
 */
int iec104_server_register_point(iec104_server_t *server, iec104_data_point_t *point);

/**
 * Update a data point value
 * @param server IEC 104 server handle
 * @param ioa Information Object Address
 * @param value New value
 * @return 0 on success, -1 on error
 */
int iec104_server_update_point(iec104_server_t *server, uint32_t ioa, iec104_data_point_t *value);

/**
 * Get a data point value
 * @param server IEC 104 server handle
 * @param ioa Information Object Address
 * @param point Output data point
 * @return 0 on success, -1 on error
 */
int iec104_server_get_point(iec104_server_t *server, uint32_t ioa, iec104_data_point_t *point);

/**
 * Process server events (call in main loop)
 * @param server IEC 104 server handle
 * @return Number of events processed or -1 on error
 */
int iec104_server_process(iec104_server_t *server);

/**
 * Free IEC 104 server resources
 * @param server IEC 104 server handle
 */
void iec104_server_free(iec104_server_t *server);

#endif /* IEC104_SERVER_H */
