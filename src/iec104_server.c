/**
 * @file iec104_server.c
 * @brief IEC 60870-5-104 Server Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "iec104_server.h"
#include "iec104_protocol.h"

#define MAX_DATA_POINTS 1024
#define MAX_CLIENTS 32

#define IEC104_K 12
#define IEC104_W 8

/* Client send/receive buffer sizes */
#define APDU_BUF_SIZE 260

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
    pthread_mutex_init(&server->lock, NULL);

    server->clients = (iec104_client_connection_t *)malloc(
        sizeof(iec104_client_connection_t) * server->max_clients);
    if (!server->clients) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(server);
        return NULL;
    }
    memset(server->clients, 0, sizeof(iec104_client_connection_t) * server->max_clients);

    for (int i = 0; i < server->max_clients; i++) {
        server->clients[i].socket = -1;
        server->clients[i].state = IEC104_CLIENT_CLOSED;
    }

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

    printf("IEC 104 server initialized (bind: %s:%d, max_clients: %d, ca: %d)\n",
           config->bind_address, config->port, server->max_clients,
           config->common_address > 0 ? config->common_address : 1);

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

    server->listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_socket < 0) {
        fprintf(stderr, "Error: Failed to create socket - %s\n", strerror(errno));
        return -1;
    }

    int opt = 1;
    if (setsockopt(server->listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "Error: Failed to set socket options - %s\n", strerror(errno));
        close(server->listen_socket);
        return -1;
    }

    if (set_nonblocking(server->listen_socket) < 0) {
        fprintf(stderr, "Error: Failed to set non-blocking - %s\n", strerror(errno));
        close(server->listen_socket);
        return -1;
    }

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
        for (int i = 0; i < server->max_clients; i++) {
            if (server->clients[i].socket >= 0) {
                close(server->clients[i].socket);
                server->clients[i].socket = -1;
                server->clients[i].state = IEC104_CLIENT_CLOSED;
            }
        }

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

    pthread_mutex_lock(&server->lock);

    int found = -1;
    for (int i = 0; i < server->data_point_count; i++) {
        if (server->data_points[i].ioa == point->ioa) {
            found = i;
            break;
        }
    }

    if (found >= 0) {
        memcpy(&server->data_points[found], point, sizeof(iec104_data_point_t));
    } else if (server->data_point_count < server->data_point_capacity) {
        memcpy(&server->data_points[server->data_point_count], point, sizeof(iec104_data_point_t));
        server->data_point_count++;
    } else {
        fprintf(stderr, "Error: Data point store is full\n");
        pthread_mutex_unlock(&server->lock);
        return -1;
    }

    pthread_mutex_unlock(&server->lock);

    printf("Registered IEC 104 data point: IOA=%u, Type=%d\n", point->ioa, point->type);
    return 0;
}

/**
 * Update a data point value
 */
int iec104_server_update_point(iec104_server_t *server, uint32_t ioa, iec104_data_point_t *value)
{
    if (!server || !value) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }

    pthread_mutex_lock(&server->lock);

    for (int i = 0; i < server->data_point_count; i++) {
        if (server->data_points[i].ioa == ioa) {
            memcpy(&server->data_points[i], value, sizeof(iec104_data_point_t));
            pthread_mutex_unlock(&server->lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&server->lock);

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

    pthread_mutex_lock(&server->lock);

    for (int i = 0; i < server->data_point_count; i++) {
        if (server->data_points[i].ioa == ioa) {
            memcpy(point, &server->data_points[i], sizeof(iec104_data_point_t));
            pthread_mutex_unlock(&server->lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&server->lock);

    fprintf(stderr, "Warning: Data point IOA=%u not found\n", ioa);
    return -1;
}

/**
 * Send an APDU to a client (records sequence numbers under server lock)
 */
static int client_send(iec104_client_connection_t *client, const uint8_t *apdu, int len)
{
    if (client->socket < 0) return -1;

    int sent = send(client->socket, apdu, len, 0);
    if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        fprintf(stderr, "Warning: send failed: %s\n", strerror(errno));
        return -1;
    }
    return sent;
}

/**
 * Send an S-format APDU acknowledging received I-frames.
 */
static int send_s_frame(iec104_client_connection_t *client, uint16_t recv_seq)
{
    uint8_t apdu[6];
    int len = iec104_encode_s_apdu(apdu, recv_seq);
    return client_send(client, apdu, len);
}

/**
 * Send a U-format APDU.
 */
static int send_u_frame(iec104_client_connection_t *client, uint8_t function)
{
    uint8_t apdu[6];
    int len = iec104_encode_u_apdu(apdu, function);
    return client_send(client, apdu, len);
}

/**
 * Send a single data point to a client.
 */
static int send_data_to_client(iec104_server_t *server,
                                 iec104_client_connection_t *client,
                                 iec104_data_point_t *point,
                                 uint8_t cause_tx)
{
    uint8_t apdu[260];
    int len;
    uint16_t ca = server->config.common_address;
    if (ca == 0) ca = 1;

    if (client->state != IEC104_CLIENT_ACTIVE) {
        return -1;
    }

    /* Check k/w window to avoid flooding. */
    if (client->unconfirmed_count >= IEC104_K) {
        return -1;
    }

    switch (point->type) {
    case IEC104_DT_BOOLEAN:
        len = iec104_encode_spi(apdu, client->send_seq, client->recv_seq,
                                point->ioa, point->value.boolean_val, point->quality, ca,
                                cause_tx);
        break;
    case IEC104_DT_UINT16:
    case IEC104_DT_INT32:
        /* 16-bit scaled values are sent as M_ME_NB_1 */
        if (point->type == IEC104_DT_UINT16) {
            len = iec104_encode_me_nb_1(apdu, client->send_seq, client->recv_seq,
                                        point->ioa, point->value.uint16_val, point->quality, ca,
                                        cause_tx);
        } else {
            /* int32: truncate to lower 16 bits for scaled value */
            len = iec104_encode_me_nb_1(apdu, client->send_seq, client->recv_seq,
                                        point->ioa, (uint16_t)(point->value.int32_val & 0xFFFF),
                                        point->quality, ca,
                                        cause_tx);
        }
        break;
    case IEC104_DT_FLOAT:
        len = iec104_encode_me_nc_1(apdu, client->send_seq, client->recv_seq,
                                    point->ioa, point->value.float_val, point->quality, ca,
                                    cause_tx);
        break;
    default:
        return -1;
    }

    if (client_send(client, apdu, len) < 0) {
        return -1;
    }

    client->send_seq = (client->send_seq + 1) & 0x7FFF;
    client->unconfirmed_count++;
    client->last_activity = time(NULL);

    /* Send S-frame if the receiver window is reached. */
    if (client->recv_seq > 0 && (client->recv_seq % IEC104_W) == 0) {
        send_s_frame(client, client->recv_seq);
    }

    return 0;
}

/**
 * Broadcast a data point to all active clients.
 */
int iec104_server_broadcast_data(iec104_server_t *server, iec104_data_point_t *point)
{
    if (!server || !point) return -1;

    int sent = 0;
    pthread_mutex_lock(&server->lock);

    for (int i = 0; i < server->max_clients; i++) {
        if (server->clients[i].state == IEC104_CLIENT_ACTIVE) {
            if (send_data_to_client(server, &server->clients[i], point,
                                IEC104_COT_SPONTANEOUS) == 0) {
                sent++;
            }
        }
    }

    pthread_mutex_unlock(&server->lock);
    return sent;
}

/**
 * Send all data points as a response to general interrogation.
 */
static int send_all_points_to_client(iec104_server_t *server,
                                     iec104_client_connection_t *client,
                                     uint16_t common_address)
{
    if (client->state != IEC104_CLIENT_ACTIVE) return -1;

    pthread_mutex_lock(&server->lock);

    for (int i = 0; i < server->data_point_count; i++) {
        iec104_data_point_t point = server->data_points[i];
        point.quality = IEC104_QDS_VALID;
        send_data_to_client(server, client, &point, IEC104_COT_RESPONSE);
    }

    pthread_mutex_unlock(&server->lock);

    /* Send ACTTERM confirmation */
    uint8_t term[260];
    int offset = 0;
    offset += 6;
    term[offset++] = IEC104_C_IC_NA_1;
    term[offset++] = 0x01;
    term[offset++] = IEC104_COT_ACTIVATION_TERMINATION;
    term[offset++] = 0x00;
    term[offset++] = (common_address & 0xFF);
    term[offset++] = ((common_address >> 8) & 0xFF);
    term[offset++] = 0x00; /* IOA low */
    term[offset++] = 0x00; /* IOA mid */
    term[offset++] = 0x00; /* IOA high */
    term[offset++] = IEC104_QOI_STATION;

    int asdu_len = offset - 6;
    iec104_encode_apdu_header(term, asdu_len, client->send_seq, client->recv_seq);
    client_send(client, term, offset);
    client->send_seq = (client->send_seq + 1) & 0x7FFF;

    return 0;
}

/**
 * Process a complete APDU from a client.
 */
static void process_apdu(iec104_server_t *server, iec104_client_connection_t *client,
                         const uint8_t *apdu, int apdu_len)
{
    uint16_t send_seq = 0, recv_seq = 0;
    int apdu_type = iec104_decode_apdu(apdu, apdu_len, &send_seq, &recv_seq);
    uint16_t ca = server->config.common_address;
    if (ca == 0) ca = 1;

    if (apdu_type < 0) {
        return;
    }

    client->last_activity = time(NULL);

    if (apdu_type == IEC104_APDU_U) {
        int func = iec104_get_u_function(apdu);

        switch (func) {
        case IEC104_STARTDT_ACT:
            client->state = IEC104_CLIENT_ACTIVE;
            client->send_seq = 0;
            client->recv_seq = 0;
            send_u_frame(client, IEC104_STARTDT_CON);
            printf("Client activated from %s\n", inet_ntoa(client->addr.sin_addr));
            break;

        case IEC104_STOPDT_ACT:
            send_u_frame(client, IEC104_STOPDT_CON);
            client->state = IEC104_CLIENT_OPEN;
            printf("Client stopped data transfer from %s\n", inet_ntoa(client->addr.sin_addr));
            break;

        case IEC104_TESTFR_ACT:
            send_u_frame(client, IEC104_TESTFR_CON);
            break;

        case IEC104_TESTFR_CON:
            /* Test frame acknowledged; nothing more to do. */
            break;

        default:
            break;
        }
    } else if (apdu_type == IEC104_APDU_S) {
        /* S-format carries Nr - the sequence number we are acknowledging. */
        client->unconfirmed_count = 0;
    } else if (apdu_type == IEC104_APDU_I) {
        /* I-format carries an ASDU. Increment our receive sequence number. */
        client->recv_seq = (client->recv_seq + 1) & 0x7FFF;

        if (client->recv_seq % IEC104_W == 0) {
            send_s_frame(client, client->recv_seq);
        }

        /* Parse ASDU for commands. Minimum APDU includes APCI + ASDU header. */
        if (apdu_len < 10) return;

        uint8_t type_id = apdu[6];
        uint8_t cot = apdu[8];
        uint16_t asdu_ca = (apdu[11] << 8) | apdu[10];
        (void)asdu_ca;

        if (type_id == IEC104_C_IC_NA_1) {
            /* General interrogation activation */
            if (cot == IEC104_COT_ACTIVATION) {
                /* Send ACTCON */
                uint8_t con[260];
                int offset = 0;
                offset += 6;
                con[offset++] = IEC104_C_IC_NA_1;
                con[offset++] = 0x01;
                con[offset++] = IEC104_COT_ACTIVATION_CON;
                con[offset++] = 0x00;
                con[offset++] = (ca & 0xFF);
                con[offset++] = ((ca >> 8) & 0xFF);
                con[offset++] = 0x00; /* IOA */
                con[offset++] = 0x00;
                con[offset++] = 0x00;
                con[offset++] = apdu_len > 15 ? apdu[15] : IEC104_QOI_STATION;

                int asdu_len = offset - 6;
                iec104_encode_apdu_header(con, asdu_len, client->send_seq, client->recv_seq);
                client_send(client, con, offset);
                client->send_seq = (client->send_seq + 1) & 0x7FFF;

                /* Send all registered data points */
                send_all_points_to_client(server, client, ca);
            }
        }
    }
}

/**
 * Read incoming bytes and extract complete APDUs.
 */
static void handle_client_connection(iec104_server_t *server,
                                     iec104_client_connection_t *client)
{
    uint8_t tmp[256];
    int n;

    if (client->socket < 0) return;

    n = recv(client->socket, tmp, sizeof(tmp), 0);
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            close(client->socket);
            client->socket = -1;
            client->state = IEC104_CLIENT_CLOSED;
        }
        return;
    }

    if (n == 0) {
        printf("Client disconnected from %s\n", inet_ntoa(client->addr.sin_addr));
        close(client->socket);
        client->socket = -1;
        client->state = IEC104_CLIENT_CLOSED;
        return;
    }

    /* Append to client buffer. */
    if (client->buffer_len + n > APDU_BUF_SIZE) {
        /* Overflow - reset buffer */
        client->buffer_len = 0;
    }
    memcpy(client->buffer + client->buffer_len, tmp, n);
    client->buffer_len += n;

    /* Extract complete APDUs: start 0x68, length byte, total len = length + 2. */
    while (client->buffer_len >= 6) {
        if (client->buffer[0] != IEC104_APCI_START) {
            /* Find next start byte */
            int i;
            for (i = 1; i < client->buffer_len; i++) {
                if (client->buffer[i] == IEC104_APCI_START) break;
            }
            if (i >= client->buffer_len) {
                client->buffer_len = 0;
                break;
            }
            memmove(client->buffer, client->buffer + i, client->buffer_len - i);
            client->buffer_len -= i;
            continue;
        }

        int apdu_len = client->buffer[1] + 2;
        if (apdu_len < 6 || apdu_len > APDU_BUF_SIZE) {
            /* Bad length - drop start byte and resync */
            memmove(client->buffer, client->buffer + 1, client->buffer_len - 1);
            client->buffer_len--;
            continue;
        }

        if (client->buffer_len < apdu_len) {
            break; /* Need more data */
        }

        process_apdu(server, client, client->buffer, apdu_len);

        memmove(client->buffer, client->buffer + apdu_len, client->buffer_len - apdu_len);
        client->buffer_len -= apdu_len;
    }
}

/**
 * Process server events
 */
int iec104_server_process(iec104_server_t *server)
{
    if (!server || !server->running) {
        return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_socket = accept(server->listen_socket, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client_socket >= 0) {
        int accepted = 0;
        for (int i = 0; i < server->max_clients; i++) {
            if (server->clients[i].socket < 0) {
                server->clients[i].socket = client_socket;
                server->clients[i].addr = client_addr;
                server->clients[i].state = IEC104_CLIENT_OPEN;
                server->clients[i].send_seq = 0;
                server->clients[i].recv_seq = 0;
                server->clients[i].unconfirmed_count = 0;
                server->clients[i].buffer_len = 0;
                server->clients[i].last_activity = time(NULL);
                set_nonblocking(client_socket);
                printf("New client connection from %s\n", inet_ntoa(client_addr.sin_addr));
                server->client_count++;
                accepted = 1;
                break;
            }
        }

        if (!accepted) {
            fprintf(stderr, "Warning: Max clients reached, rejecting connection\n");
            close(client_socket);
        }
    }

    for (int i = 0; i < server->max_clients; i++) {
        if (server->clients[i].socket >= 0) {
            handle_client_connection(server, &server->clients[i]);

            if (server->clients[i].socket < 0) {
                server->client_count--;
                if (server->client_count < 0) server->client_count = 0;
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
    if (!server) return;

    iec104_server_stop(server);

    if (server->clients) {
        free(server->clients);
    }

    if (server->data_points) {
        free(server->data_points);
    }

    pthread_mutex_destroy(&server->lock);
    free(server);
}
