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
#include "modbus_master.h"
#include "iec104_server.h"

#define GATEWAY_NAME "Modbus-IEC104-Gateway"
#define GATEWAY_VERSION "1.0.0"
#define DEFAULT_POLL_INTERVAL 1000  /* milliseconds */

typedef struct {
    modbus_device_t *modbus_dev;
    iec104_server_t *iec104_srv;
    int running;
    int poll_interval;
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
 * Modbus polling thread
 */
void *modbus_polling_thread(void *arg)
{
    gateway_context_t *ctx = (gateway_context_t *)arg;
    uint16_t registers[100];
    uint8_t coils[100];

    printf("Modbus polling thread started\n");

    while (ctx->running) {
        if (!ctx->modbus_dev || !ctx->modbus_dev->connected) {
            printf("Modbus device not connected, waiting...\n");
            sleep(2);
            
            /* Try to reconnect */
            if (ctx->modbus_dev && modbus_master_connect(ctx->modbus_dev) == 0) {
                printf("Modbus reconnected\n");
            }
            continue;
        }

        /* Example: Read holding registers (address 0, count 10) */
        int num_regs = modbus_master_read_holding_registers(
            ctx->modbus_dev, 0, 10, registers
        );
        if (num_regs > 0) {
            printf("Read %d holding registers:\n", num_regs);
            for (int i = 0; i < num_regs; i++) {
                printf("  Reg[%d] = 0x%04X (%d)\n", i, registers[i], registers[i]);

                /* Update IEC 104 data points */
                if (ctx->iec104_srv) {
                    iec104_data_point_t point;
                    point.ioa = 1000 + i;  /* IOA = 1000 + register index */
                    point.type = IEC104_DT_UINT16;
                    point.value.uint16_val = registers[i];
                    point.timestamp = time(NULL);
                    point.quality = 0;

                    iec104_server_update_point(ctx->iec104_srv, point.ioa, &point);
                }
            }
        }

        /* Example: Read coils (address 0, count 10) */
        int num_coils = modbus_master_read_coils(
            ctx->modbus_dev, 0, 10, coils
        );
        if (num_coils > 0) {
            printf("Read %d coils:\n", num_coils);
            for (int i = 0; i < num_coils; i++) {
                printf("  Coil[%d] = %d\n", i, coils[i]);

                /* Update IEC 104 data points */
                if (ctx->iec104_srv) {
                    iec104_data_point_t point;
                    point.ioa = 2000 + i;  /* IOA = 2000 + coil index */
                    point.type = IEC104_DT_BOOLEAN;
                    point.value.boolean_val = coils[i];
                    point.timestamp = time(NULL);
                    point.quality = 0;

                    iec104_server_update_point(ctx->iec104_srv, point.ioa, &point);
                }
            }
        }

        /* Poll interval */
        usleep(ctx->poll_interval * 1000);
    }

    printf("Modbus polling thread stopped\n");
    return NULL;
}

/**
 * Initialize gateway
 */
int gateway_init(gateway_context_t *ctx)
{
    if (!ctx) {
        fprintf(stderr, "Error: Invalid context\n");
        return -1;
    }

    /* Initialize Modbus Master */
    modbus_config_t modbus_cfg;
    memset(&modbus_cfg, 0, sizeof(modbus_cfg));
    modbus_cfg.host = "127.0.0.1";
    modbus_cfg.port = 502;

    ctx->modbus_dev = modbus_master_init(&modbus_cfg);
    if (!ctx->modbus_dev) {
        fprintf(stderr, "Error: Failed to initialize Modbus master\n");
        return -1;
    }

    /* Connect to Modbus device */
    if (modbus_master_connect(ctx->modbus_dev) != 0) {
        printf("Warning: Failed to connect to Modbus device (will retry)\n");
    }

    /* Initialize IEC 104 Server */
    iec104_config_t iec104_cfg;
    memset(&iec104_cfg, 0, sizeof(iec104_cfg));
    iec104_cfg.bind_address = "0.0.0.0";
    iec104_cfg.port = 2404;  /* Standard IEC 104 port */
    iec104_cfg.max_clients = 10;

    ctx->iec104_srv = iec104_server_init(&iec104_cfg);
    if (!ctx->iec104_srv) {
        fprintf(stderr, "Error: Failed to initialize IEC 104 server\n");
        modbus_master_free(ctx->modbus_dev);
        return -1;
    }

    /* Start IEC 104 Server */
    if (iec104_server_start(ctx->iec104_srv) != 0) {
        fprintf(stderr, "Error: Failed to start IEC 104 server\n");
        iec104_server_free(ctx->iec104_srv);
        modbus_master_free(ctx->modbus_dev);
        return -1;
    }

    /* Register IEC 104 data points */
    for (int i = 0; i < 10; i++) {
        iec104_data_point_t point;
        point.ioa = 1000 + i;
        point.type = IEC104_DT_UINT16;
        point.value.uint16_val = 0;
        point.timestamp = time(NULL);
        point.quality = 0;
        iec104_server_register_point(ctx->iec104_srv, &point);
    }

    ctx->running = 1;
    ctx->poll_interval = DEFAULT_POLL_INTERVAL;

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

    /* Start Modbus polling thread */
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

    /* Wait for modbus thread */
    if (ctx->modbus_thread) {
        pthread_join(ctx->modbus_thread, NULL);
    }

    /* Stop IEC 104 server */
    if (ctx->iec104_srv) {
        iec104_server_stop(ctx->iec104_srv);
        iec104_server_free(ctx->iec104_srv);
    }

    /* Disconnect Modbus */
    if (ctx->modbus_dev) {
        modbus_master_free(ctx->modbus_dev);
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

    /* Parse command line arguments */
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
    }

    /* Allocate gateway context */
    g_ctx = (gateway_context_t *)malloc(sizeof(gateway_context_t));
    if (!g_ctx) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }
    memset(g_ctx, 0, sizeof(gateway_context_t));

    /* Register signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize gateway */
    if (gateway_init(g_ctx) != 0) {
        fprintf(stderr, "Error: Failed to initialize gateway\n");
        free(g_ctx);
        return 1;
    }

    /* Start gateway */
    if (gateway_start(g_ctx) != 0) {
        fprintf(stderr, "Error: Failed to start gateway\n");
        gateway_stop(g_ctx);
        free(g_ctx);
        return 1;
    }

    /* Main loop */
    printf("Gateway running. Press Ctrl+C to stop.\n");
    while (g_ctx->running) {
        /* Process IEC 104 server events */
        if (g_ctx->iec104_srv) {
            iec104_server_process(g_ctx->iec104_srv);
        }
        sleep(1);
    }

    /* Cleanup */
    gateway_stop(g_ctx);
    free(g_ctx);

    return 0;
}
