/**
 * @file gateway.c
 * @brief Gateway Bridge - Connects Modbus Master with IEC 104 Server
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h>
#include "modbus_master.h"
#include "iec104_protocol.h"
#include "iec104_server.h"
#include "config_parser.h"

#define GATEWAY_NAME "Modbus-IEC104-Gateway"
#define GATEWAY_VERSION "1.0.0"
#define DEFAULT_POLL_INTERVAL 1000  /* milliseconds */

typedef struct {
    modbus_device_t *modbus_dev;
    iec104_server_t *iec104_srv;
    gateway_config_t *config;
    int running;
    pthread_t modbus_thread;
} gateway_context_t;

static gateway_context_t *g_ctx = NULL;

/**
 * Print usage information
 */
void print_usage(const char *program)
{
    printf("Usage: %s [options]\n", program);
    printf("Options:\n");
    printf("  -c <config>  Configuration file\n");
    printf("  -h           Show this help message\n");
    printf("  -v           Show version\n");
}

/**
 * Print version information
 */
void print_version(void)
{
    printf("%s v%s\n", GATEWAY_NAME, GATEWAY_VERSION);
}

/**
 * Signal handler for graceful shutdown
 */
void signal_handler(int sig)
{
    printf("\nReceived signal %d, shutting down...\n", sig);
    if (g_ctx) {
        g_ctx->running = 0;
    }
}

/**
 * Combine two 16-bit Modbus registers into a 32-bit value.
 * Assumes little-endian word order (least significant word first).
 */
static uint32_t combine_registers(uint16_t low, uint16_t high)
{
    return ((uint32_t)high << 16) | low;
}

/**
 * Read a single mapping from Modbus and update the corresponding IEC104 point.
 */
static int poll_mapping(gateway_context_t *ctx, data_mapping_t *mapping)
{
    uint16_t regs[2];
    uint8_t bits[1];
    int32_t int32_val = 0;
    uint32_t uint32_val = 0;
    float float_val = 0.0f;
    uint16_t uint16_val = 0;
    uint8_t boolean_val = 0;
    int value_set = 0;

    switch (mapping->modbus_function) {
    case 1: /* coil */
        if (modbus_master_read_coils(ctx->modbus_dev, mapping->modbus_address, 1, bits) != 1) {
            return -1;
        }
        boolean_val = bits[0] ? 1 : 0;
        value_set = 1;
        break;

    case 2: /* discrete input */
        if (modbus_master_read_discrete_inputs(ctx->modbus_dev, mapping->modbus_address, 1, bits) != 1) {
            return -1;
        }
        boolean_val = bits[0] ? 1 : 0;
        value_set = 1;
        break;

    case 3: /* holding register */
    case 4: /* input register */
    {
        int (*read_fn)(modbus_device_t *, int, int, uint16_t *) =
            (mapping->modbus_function == 3)
                ? modbus_master_read_holding_registers
                : modbus_master_read_input_registers;

        switch (mapping->data_type) {
        case MAP_BOOLEAN:
            if (read_fn(ctx->modbus_dev, mapping->modbus_address, 1, regs) != 1) return -1;
            boolean_val = regs[0] ? 1 : 0;
            value_set = 1;
            break;

        case MAP_INT16:
            if (read_fn(ctx->modbus_dev, mapping->modbus_address, 1, regs) != 1) return -1;
            int32_val = (int16_t)regs[0];
            value_set = 1;
            break;

        case MAP_UINT16:
            if (read_fn(ctx->modbus_dev, mapping->modbus_address, 1, regs) != 1) return -1;
            uint16_val = regs[0];
            value_set = 1;
            break;

        case MAP_INT32:
        case MAP_UINT32:
        case MAP_FLOAT:
            if (read_fn(ctx->modbus_dev, mapping->modbus_address, 2, regs) != 2) return -1;
            uint32_val = combine_registers(regs[0], regs[1]);
            if (mapping->data_type == MAP_INT32) {
                int32_val = (int32_t)uint32_val;
            } else if (mapping->data_type == MAP_FLOAT) {
                memcpy(&float_val, &uint32_val, sizeof(float_val));
            }
            value_set = 1;
            break;
        }
        break;
    }

    default:
        return -1;
    }

    if (!value_set) return -1;

    /* Apply scale and offset */
    if (mapping->data_type == MAP_FLOAT) {
        float_val = float_val * mapping->scale + mapping->offset;
    } else if (mapping->data_type != MAP_BOOLEAN) {
        float scaled = 0.0f;
        switch (mapping->data_type) {
        case MAP_INT16:
        case MAP_INT32:
            scaled = (float)int32_val * mapping->scale + mapping->offset;
            int32_val = (int32_t)scaled;
            break;
        case MAP_UINT16:
            scaled = (float)uint16_val * mapping->scale + mapping->offset;
            uint16_val = (uint16_t)scaled;
            break;
        case MAP_UINT32:
            scaled = (float)uint32_val * mapping->scale + mapping->offset;
            uint32_val = (uint32_t)scaled;
            break;
        default:
            break;
        }
    }

    /* Build data point */
    iec104_data_point_t point;
    memset(&point, 0, sizeof(point));
    point.ioa = mapping->iec104_ioa;
    point.type = mapping_to_iec104_type(mapping->data_type);
    point.timestamp = time(NULL);
    point.quality = IEC104_QDS_VALID;

    switch (mapping->data_type) {
    case MAP_BOOLEAN:
        point.value.boolean_val = boolean_val;
        break;
    case MAP_INT16:
    case MAP_INT32:
        point.value.int32_val = int32_val;
        break;
    case MAP_UINT16:
        point.value.uint16_val = uint16_val;
        break;
    case MAP_UINT32:
        point.value.int32_val = (int32_t)uint32_val;
        break;
    case MAP_FLOAT:
        point.value.float_val = float_val;
        break;
    }

    /* Update and broadcast */
    if (iec104_server_update_point(ctx->iec104_srv, point.ioa, &point) == 0) {
        iec104_server_broadcast_data(ctx->iec104_srv, &point);
    }

    return 0;
}

/**
 * Modbus polling thread
 */
void *modbus_polling_thread(void *arg)
{
    gateway_context_t *ctx = (gateway_context_t *)arg;

    printf("Modbus polling thread started\n");

    /* Per-mapping last-read timestamps */
    time_t *last_read = (time_t *)calloc(ctx->config->mapping_count, sizeof(time_t));
    if (!last_read) {
        fprintf(stderr, "Error: Failed to allocate last-read tracker\n");
        return NULL;
    }

    while (ctx->running) {
        if (!ctx->modbus_dev || !ctx->modbus_dev->connected) {
            if (ctx->modbus_dev) {
                if (modbus_master_connect(ctx->modbus_dev) == 0) {
                    printf("Modbus reconnected\n");
                }
            }
            sleep(2);
            continue;
        }

        time_t now = time(NULL);
        int polled = 0;

        for (int i = 0; i < ctx->config->mapping_count; i++) {
            data_mapping_t *m = &ctx->config->mappings[i];
            int interval = (m->read_interval_ms > 0)
                ? m->read_interval_ms
                : ctx->config->modbus_poll_interval_ms;

            if (difftime(now, last_read[i]) * 1000 >= interval) {
                if (poll_mapping(ctx, m) == 0) {
                    last_read[i] = now;
                    polled++;
                }
            }
        }

        if (!polled) {
            usleep(50 * 1000);
        }
    }

    free(last_read);
    printf("Modbus polling thread stopped\n");
    return NULL;
}

/**
 * Initialize gateway
 */
int gateway_init(gateway_context_t *ctx, const char *config_file)
{
    if (!ctx) {
        fprintf(stderr, "Error: Invalid context\n");
        return -1;
    }

    ctx->config = (gateway_config_t *)malloc(sizeof(gateway_config_t));
    if (!ctx->config) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return -1;
    }

    if (config_file) {
        if (parse_config_file(config_file, ctx->config) != 0) {
            free(ctx->config);
            return -1;
        }
    } else {
        memset(ctx->config, 0, sizeof(gateway_config_t));
        strcpy(ctx->config->log_level, "INFO");
        strcpy(ctx->config->modbus.host, "127.0.0.1");
        ctx->config->modbus.port = 502;
        ctx->config->modbus.parity = 'N';
        ctx->config->modbus.data_bits = 8;
        ctx->config->modbus.stop_bits = 1;
        ctx->config->modbus.slave_id = 1;
        ctx->config->modbus_slave_id = 1;
        ctx->config->modbus_poll_interval_ms = DEFAULT_POLL_INTERVAL;
        strcpy(ctx->config->iec104_bind_address, "0.0.0.0");
        ctx->config->iec104_port = 2404;
        ctx->config->iec104_max_clients = 10;
        ctx->config->iec104_common_address = 1;
    }

    printf("Modbus target: %s:%d (slave=%d)\n",
           ctx->config->modbus.host,
           ctx->config->modbus.port,
           ctx->config->modbus_slave_id);

    /* Initialize Modbus Master */
    ctx->modbus_dev = modbus_master_init(&ctx->config->modbus);
    if (!ctx->modbus_dev) {
        fprintf(stderr, "Error: Failed to initialize Modbus master\n");
        free(ctx->config);
        return -1;
    }

    if (modbus_master_connect(ctx->modbus_dev) != 0) {
        printf("Warning: Failed to connect to Modbus device (will retry)\n");
    }

    /* Initialize IEC 104 Server */
    iec104_config_t iec104_cfg;
    memset(&iec104_cfg, 0, sizeof(iec104_cfg));
    snprintf(iec104_cfg.bind_address, sizeof(iec104_cfg.bind_address), "%s",
             ctx->config->iec104_bind_address);
    iec104_cfg.port = ctx->config->iec104_port;
    iec104_cfg.max_clients = ctx->config->iec104_max_clients;
    iec104_cfg.common_address = ctx->config->iec104_common_address;

    ctx->iec104_srv = iec104_server_init(&iec104_cfg);
    if (!ctx->iec104_srv) {
        fprintf(stderr, "Error: Failed to initialize IEC 104 server\n");
        modbus_master_free(ctx->modbus_dev);
        free(ctx->config);
        return -1;
    }

    if (iec104_server_start(ctx->iec104_srv) != 0) {
        fprintf(stderr, "Error: Failed to start IEC 104 server\n");
        iec104_server_free(ctx->iec104_srv);
        modbus_master_free(ctx->modbus_dev);
        free(ctx->config);
        return -1;
    }

    /* Register IEC 104 data points from config mappings */
    for (int i = 0; i < ctx->config->mapping_count; i++) {
        data_mapping_t *m = &ctx->config->mappings[i];
        iec104_data_point_t point;
        memset(&point, 0, sizeof(point));
        point.ioa = m->iec104_ioa;
        point.type = mapping_to_iec104_type(m->data_type);
        point.timestamp = time(NULL);
        point.quality = IEC104_QDS_VALID;
        iec104_server_register_point(ctx->iec104_srv, &point);
    }

    ctx->running = 1;
    return 0;
}

/**
 * Start gateway
 */
int gateway_start(gateway_context_t *ctx)
{
    if (!ctx) {
        fprintf(stderr, "Error: Invalid context\n");
        return -1;
    }

    if (pthread_create(&ctx->modbus_thread, NULL, modbus_polling_thread, ctx) != 0) {
        fprintf(stderr, "Error: Failed to create modbus polling thread\n");
        return -1;
    }

    printf("Gateway started\n");
    return 0;
}

/**
 * Stop gateway
 */
void gateway_stop(gateway_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    ctx->running = 0;

    if (ctx->modbus_thread) {
        pthread_join(ctx->modbus_thread, NULL);
    }

    if (ctx->iec104_srv) {
        iec104_server_stop(ctx->iec104_srv);
        iec104_server_free(ctx->iec104_srv);
    }

    if (ctx->modbus_dev) {
        modbus_master_free(ctx->modbus_dev);
    }

    if (ctx->config) {
        free(ctx->config);
    }

    printf("Gateway stopped\n");
}

/**
 * Main function
 */
int main(int argc, char *argv[])
{
    const char *config_file = NULL;
    int opt;

    printf("%s v%s\n\n", GATEWAY_NAME, GATEWAY_VERSION);

    while ((opt = getopt(argc, argv, "c:hv")) != -1) {
        switch (opt) {
        case 'c':
            config_file = optarg;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        case 'v':
            print_version();
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (config_file) {
        printf("Using configuration file: %s\n", config_file);
    } else {
        printf("No configuration file; using defaults.\n");
    }

    g_ctx = (gateway_context_t *)malloc(sizeof(gateway_context_t));
    if (!g_ctx) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }
    memset(g_ctx, 0, sizeof(gateway_context_t));

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (gateway_init(g_ctx, config_file) != 0) {
        fprintf(stderr, "Error: Failed to initialize gateway\n");
        free(g_ctx);
        return 1;
    }

    if (gateway_start(g_ctx) != 0) {
        fprintf(stderr, "Error: Failed to start gateway\n");
        gateway_stop(g_ctx);
        free(g_ctx);
        return 1;
    }

    printf("Gateway running. Press Ctrl+C to stop.\n");
    while (g_ctx->running) {
        if (g_ctx->iec104_srv) {
            iec104_server_process(g_ctx->iec104_srv);
        }
        usleep(100 * 1000);
    }

    gateway_stop(g_ctx);
    free(g_ctx);

    return 0;
}
