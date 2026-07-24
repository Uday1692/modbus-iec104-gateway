/**
 * @file iec104_server.h
 * @brief IEC 104 Server Interface with Full Protocol Support
 */

#ifndef IEC104_SERVER_H
#define IEC104_SERVER_H

#include <stdint.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct {
    char *bind_address;
    int port;
    int max_clients;
} iec104_config_t;

typedef enum {
    IEC104_DT_BOOLEAN,
    IEC104_DT_INT32,
    IEC104_DT_UINT16,
    IEC104_DT_FLOAT,
} iec104_data_type_t;

typedef struct {
    uint32_t ioa;              /* Information Object Address */
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

typedef enum {
    IEC104_CLIENT_CLOSED = 0,
    IEC104_CLIENT_OPENING = 1,
    IEC104_CLIENT_OPEN = 2,
    IEC104_CLIENT_ACTIVE = 3
} iec104_client_state_t;

typedef struct {
    int socket;
    struct sockaddr_in addr;
    iec104_client_state_t state;
    uint16_t send_seq;         /* Send sequence number */
    uint16_t recv_seq;         /* Receive sequence number */
    uint8_t unconfirmed_count; /* Unconfirmed APDUs */
    time_t last_activity;
    uint8_t buffer[260];       /* Buffer for APDU data */
    int buffer_len;
} iec104_client_connection_t;

typedef struct iec104_server {
    int listen_socket;
    iec104_config_t config;
    int running;
    
    /* Client management */
    iec104_client_connection_t *clients;
    int client_count;
    int max_clients;
    
    /* Data point storage */
    iec104_data_point_t *data_points;
    int data_point_count;
    int data_point_capacity;
} iec104_server_t;

/**
 * Initialize IEC 104 server
 */
iec104_server_t *iec104_server_init(iec104_config_t *config);

/**
 * Start IEC 104 server
 */
int iec104_server_start(iec104_server_t *server);

/**
 * Stop IEC 104 server
 */
int iec104_server_stop(iec104_server_t *server);

/**
 * Register a data point
 */
int iec104_server_register_point(iec104_server_t *server, iec104_data_point_t *point);

/**
 * Update a data point value
 */
int iec104_server_update_point(iec104_server_t *server, uint32_t ioa, iec104_data_point_t *value);

/**
 * Get a data point value
 */
int iec104_server_get_point(iec104_server_t *server, uint32_t ioa, iec104_data_point_t *point);

/**
 * Process server events (accept connections, send/receive data)
 */
int iec104_server_process(iec104_server_t *server);

/**
 * Send data to all connected clients
 */
int iec104_server_broadcast_data(iec104_server_t *server, iec104_data_point_t *point);

/**
 * Free server resources
 */
void iec104_server_free(iec104_server_t *server);

#endif /* IEC104_SERVER_H */
