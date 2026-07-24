/**
 * @file iec104_server.c
 * @brief IEC 104 Server Implementation with Full Protocol Support
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "iec104_server.h"
#include "iec104_protocol.h"

#define MAX_DATA_POINTS 1024
#define MAX_CLIENTS 32

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

    memset(server, 0, sizeof(iec104_server_t));
    memcpy(&server->config, config, sizeof(iec104_config_t));
    server->listen_socket = -1;
    server->running = 0;
    server->max_clients = (config->max_clients > 0) ? config->max_clients : MAX_CLIENTS;
    server->client_count = 0;

    /* Allocate client connections array */
    server->clients = (iec104_client_connection_t *)malloc(
        sizeof(iec104_client_connection_t) * server->max_clients);
    if (!server->clients) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(server);
        return NULL;
    }
    memset(server->clients, 0, sizeof(iec104_client_connection_t) * server->max_clients);

    /* Initialize client sockets */
    for (int i = 0; i < server->max_clients; i++) {
        server->clients[i].socket = -1;
        server->clients[i].state = IEC104_CLIENT_CLOSED;
    }

    /* Allocate data point storage */
    server->data_points = (iec104_data_point_t *)malloc(
        sizeof(iec104_data_point_t) * MAX_DATA_POINTS);
    if (!server->data_points) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(server->clients);
        free(server);
        return NULL;
    }
    memset(server->data_points, 0, sizeof(iec104_data_point_t) * MAX_DATA_POINTS);
    server->data_point_capacity = MAX_DATA_POINTS;
    server->data_point_count = 0;

    printf("IEC 104 server initialized (bind: %s:%d, max_clients: %d)\n",
           config->bind_address, config->port, server->max_clients);

    return server;
}

/**
 * Set socket to non-blocking mode
 */
static int set_nonblocking(int sock)
{
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
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

    /* Make non-blocking */
    if (set_nonblocking(server->listen_socket) < 0) {
        fprintf(stderr, "Error: Failed to set non-blocking - %s\n", strerror(errno));
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
    if (listen(server->listen_socket, server->max_clients) < 0) {
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
        /* Close all client connections */
        for (int i = 0; i < server->max_clients; i++) {
            if (server->clients[i].socket >= 0) {
                close(server->clients[i].socket);
                server->clients[i].socket = -1;
                server->clients[i].state = IEC104_CLIENT_CLOSED;
            }
        }

        /* Close listening socket */
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

    if (server->data_point_count >= server->data_point_capacity) {
        fprintf(stderr, "Error: Data point store is full\n");
        return -1;
    }

    memcpy(&server->data_points[server->data_point_count], point, sizeof(iec104_data_point_t));
    server->data_point_count++;

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

    for (int i = 0; i < server->data_point_count; i++) {
        if (server->data_points[i].ioa == ioa) {
            memcpy(&server->data_points[i], value, sizeof(iec104_data_point_t));
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

    for (int i = 0; i < server->data_point_count; i++) {
        if (server->data_points[i].ioa == ioa) {
            memcpy(point, &server->data_points[i], sizeof(iec104_data_point_t));
            return 0;
        }
    }

    fprintf(stderr, "Warning: Data point IOA=%u not found\n", ioa);
    return -1;
}

/**
 * Handle client connection state machine
 */
static void handle_client_connection(iec104_server_t *server, iec104_client_connection_t *client)
{
    uint8_t apdu[260];
    int len;
    uint16_t send_seq, recv_seq;

    switch (client->state) {
    case IEC104_CLIENT_OPENING:
        /* Send STARTDT_ACT response */
        len = iec104_encode_u_apdu(apdu, IEC104_STARTDT_CON);
        if (send(client->socket, apdu, len, 0) < 0) {
            fprintf(stderr, "Warning: Failed to send STARTDT_CON\n");
            close(client->socket);
            client->socket = -1;
            client->state = IEC104_CLIENT_CLOSED;
        } else {
            client->state = IEC104_CLIENT_ACTIVE;
            client->send_seq = 0;
            client->recv_seq = 0;
            printf("Client activated from %s\n", inet_ntoa(client->addr.sin_addr));
        }
        break;

    case IEC104_CLIENT_ACTIVE:
        /* Receive and process APDUs */
        len = recv(client->socket, apdu, sizeof(apdu), 0);
        if (len <= 0) {
            if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                close(client->socket);
                client->socket = -1;
                client->state = IEC104_CLIENT_CLOSED;
                printf("Client disconnected\n");
            }
        } else {
            /* Decode APDU */
            int apdu_type = iec104_decode_apdu(apdu, len, &send_seq, &recv_seq);
            if (apdu_type == IEC104_APDU_U) {
                int func = iec104_get_u_function(apdu);
                if (func == IEC104_STOPDT_ACT) {
                    len = iec104_encode_u_apdu(apdu, IEC104_STOPDT_CON);
                    send(client->socket, apdu, len, 0);
                    client->state = IEC104_CLIENT_CLOSED;
                    close(client->socket);
                    client->socket = -1;
                }
                /* Handle TESTFR_ACT */
                else if (func == IEC104_TESTFR_ACT) {
                    len = iec104_encode_u_apdu(apdu, IEC104_TESTFR_CON);
                    send(client->socket, apdu, len, 0);
                }
            }
            /* Handle S-format (confirmation) */
            else if (apdu_type == IEC104_APDU_S) {
                client->unconfirmed_count = 0;
            }
        }
        break;

    default:
        break;
    }
}

/**
 * Send data to a specific client
 */
static int send_data_to_client(iec104_client_connection_t *client,
                                iec104_data_point_t *point)
{
    uint8_t apdu[260];
    int len;

    if (client->state != IEC104_CLIENT_ACTIVE) {
        return -1;
    }

    /* Encode based on data type */
    switch (point->type) {
    case IEC104_DT_BOOLEAN:
        len = iec104_encode_spi(apdu, client->send_seq, client->recv_seq,
                                point->ioa, point->value.boolean_val, point->quality, 1);
        break;
    case IEC104_DT_UINT16:
        len = iec104_encode_me_na_1(apdu, client->send_seq, client->recv_seq,
                                    point->ioa, point->value.uint16_val, point->quality, 1);
        break;
    case IEC104_DT_FLOAT:
        len = iec104_encode_me_tf_1(apdu, client->send_seq, client->recv_seq,
                                    point->ioa, point->value.float_val, point->quality, 1);
        break;
    default:
        return -1;
    }

    if (send(client->socket, apdu, len, 0) > 0) {
        client->send_seq = (client->send_seq + 1) & 0x7FFF;
        client->unconfirmed_count++;
        return 0;
    }

    return -1;
}

/**
 * Broadcast data to all connected clients
 */
int iec104_server_broadcast_data(iec104_server_t *server, iec104_data_point_t *point)
{
    if (!server || !point) {
        return -1;
    }

    int sent = 0;
    for (int i = 0; i < server->max_clients; i++) {
        if (server->clients[i].state == IEC104_CLIENT_ACTIVE) {
            if (send_data_to_client(&server->clients[i], point) == 0) {
                sent++;
            }
        }
    }

    return sent;
}

/**
 * Process server events
 */
int iec104_server_process(iec104_server_t *server)
{
    if (!server || !server->running) {
        return -1;
    }

    /* Accept new connections */
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_socket = accept(server->listen_socket, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client_socket >= 0) {
        /* Find empty slot */
        for (int i = 0; i < server->max_clients; i++) {
            if (server->clients[i].socket < 0) {
                server->clients[i].socket = client_socket;
                server->clients[i].addr = client_addr;
                server->clients[i].state = IEC104_CLIENT_OPENING;
                server->clients[i].last_activity = time(NULL);
                set_nonblocking(client_socket);
                printf("New client connection from %s\n", inet_ntoa(client_addr.sin_addr));
                server->client_count++;
                break;
            }
        }
    }

    /* Handle all client connections */
    for (int i = 0; i < server->max_clients; i++) {
        if (server->clients[i].socket >= 0) {
            handle_client_connection(server, &server->clients[i]);
            
            if (server->clients[i].socket < 0) {
                server->client_count--;
            }
        }
    }

    return server->client_count;
}

/**
 * Free server resources
 */
void iec104_server_free(iec104_server_t *server)
{
    if (!server) {
        return;
    }

    iec104_server_stop(server);

    if (server->clients) {
        free(server->clients);
    }

    if (server->data_points) {
        free(server->data_points);
    }

    free(server);
}
