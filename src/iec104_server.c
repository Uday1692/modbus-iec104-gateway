/**
 * @file iec104_server.c
 * @brief IEC 104 Server Implementation
 * 
 * This is a basic stub implementation. Full implementation would use libasdu
 * or a similar IEC 104 protocol library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "iec104_server.h"

#define MAX_DATA_POINTS 1024

typedef struct {
    iec104_data_point_t *points;
    int point_count;
    int point_capacity;
} data_point_store_t;

/**
 * Initialize IEC 104 server
 */
iec104_server_t *iec104_server_init(iec104_config_t *config)
{
    if (!config) {
        fprintf(stderr, "Error: Invalid configuration\n");
        return NULL;
    }

    iec104_server_t *server = (iec104_server_t *)malloc(sizeof(iec104_server_t));
    if (!server) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    memcpy(&server->config, config, sizeof(iec104_config_t));
    server->listen_socket = -1;
    server->running = 0;

    /* Initialize data point store */
    data_point_store_t *store = (data_point_store_t *)malloc(sizeof(data_point_store_t));
    if (!store) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(server);
        return NULL;
    }

    store->points = (iec104_data_point_t *)malloc(sizeof(iec104_data_point_t) * MAX_DATA_POINTS);
    if (!store->points) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(store);
        free(server);
        return NULL;
    }

    store->point_count = 0;
    store->point_capacity = MAX_DATA_POINTS;
    server->data_points = (void *)store;

    printf("IEC 104 server initialized (bind: %s:%d, max_clients: %d)\n",
           config->bind_address, config->port, config->max_clients);

    return server;
}

/**
 * Start IEC 104 server
 */
int iec104_server_start(iec104_server_t *server)
{
    if (!server) {
        fprintf(stderr, "Error: Invalid server\n");
        return -1;
    }

    if (server->running) {
        printf("Server is already running\n");
        return 0;
    }

    /* Create listening socket */
    server->listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_socket < 0) {
        fprintf(stderr, "Error: Failed to create socket - %s\n", strerror(errno));
        return -1;
    }

    /* Set socket options */
    int opt = 1;
    if (setsockopt(server->listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "Error: Failed to set socket options - %s\n", strerror(errno));
        close(server->listen_socket);
        return -1;
    }

    /* Bind socket */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->config.port);
    
    if (inet_pton(AF_INET, server->config.bind_address, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Error: Invalid bind address\n");
        close(server->listen_socket);
        return -1;
    }

    if (bind(server->listen_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Error: Failed to bind socket - %s\n", strerror(errno));
        close(server->listen_socket);
        return -1;
    }

    /* Listen for connections */
    if (listen(server->listen_socket, server->config.max_clients) < 0) {
        fprintf(stderr, "Error: Failed to listen - %s\n", strerror(errno));
        close(server->listen_socket);
        return -1;
    }

    server->running = 1;
    printf("IEC 104 server started on %s:%d\n", 
           server->config.bind_address, server->config.port);

    return 0;
}

/**
 * Stop IEC 104 server
 */
int iec104_server_stop(iec104_server_t *server)
{
    if (!server) {
        fprintf(stderr, "Error: Invalid server\n");
        return -1;
    }

    if (server->running) {
        if (server->listen_socket >= 0) {
            close(server->listen_socket);
            server->listen_socket = -1;
        }
        server->running = 0;
        printf("IEC 104 server stopped\n");
    }

    return 0;
}

/**
 * Register a data point
 */
int iec104_server_register_point(iec104_server_t *server, iec104_data_point_t *point)
{
    if (!server || !point) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }

    data_point_store_t *store = (data_point_store_t *)server->data_points;
    if (!store) {
        fprintf(stderr, "Error: Data store not initialized\n");
        return -1;
    }

    if (store->point_count >= store->point_capacity) {
        fprintf(stderr, "Error: Data point store is full\n");
        return -1;
    }

    memcpy(&store->points[store->point_count], point, sizeof(iec104_data_point_t));
    store->point_count++;

    printf("Registered IEC 104 data point: IOA=%u, Type=%d\n", point->ioa, point->type);

    return 0;
}

/**
 * Update a data point
 */
int iec104_server_update_point(iec104_server_t *server, uint32_t ioa, iec104_data_point_t *value)
{
    if (!server || !value) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }

    data_point_store_t *store = (data_point_store_t *)server->data_points;
    if (!store) {
        fprintf(stderr, "Error: Data store not initialized\n");
        return -1;
    }

    for (int i = 0; i < store->point_count; i++) {
        if (store->points[i].ioa == ioa) {
            memcpy(&store->points[i], value, sizeof(iec104_data_point_t));
            return 0;
        }
    }

    fprintf(stderr, "Warning: Data point IOA=%u not found\n", ioa);
    return -1;
}

/**
 * Get a data point
 */
int iec104_server_get_point(iec104_server_t *server, uint32_t ioa, iec104_data_point_t *point)
{
    if (!server || !point) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }

    data_point_store_t *store = (data_point_store_t *)server->data_points;
    if (!store) {
        fprintf(stderr, "Error: Data store not initialized\n");
        return -1;
    }

    for (int i = 0; i < store->point_count; i++) {
        if (store->points[i].ioa == ioa) {
            memcpy(point, &store->points[i], sizeof(iec104_data_point_t));
            return 0;
        }
    }

    fprintf(stderr, "Warning: Data point IOA=%u not found\n", ioa);
    return -1;
}

/**
 * Process server events
 */
int iec104_server_process(iec104_server_t *server)
{
    if (!server || !server->running) {
        return -1;
    }

    /* TODO: Implement actual IEC 104 protocol handling */
    /* This would include:
     * - Accepting client connections
     * - Handling IEC 104 APDU frames
     * - Sending data to connected clients
     */

    return 0;
}

/**
 * Free IEC 104 server resources
 */
void iec104_server_free(iec104_server_t *server)
{
    if (!server) {
        return;
    }

    iec104_server_stop(server);

    data_point_store_t *store = (data_point_store_t *)server->data_points;
    if (store) {
        if (store->points) {
            free(store->points);
        }
        free(store);
    }

    free(server);
}
