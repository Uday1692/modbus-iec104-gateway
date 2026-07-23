/**
 * @file modbus_master.c
 * @brief Modbus Master Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <modbus.h>
#include "modbus_master.h"

#define MODBUS_READ_TIMEOUT_SEC 5
#define MODBUS_READ_TIMEOUT_USEC 0

/**
 * Initialize Modbus master device
 */
modbus_device_t *modbus_master_init(modbus_config_t *config)
{
    if (!config) {
        fprintf(stderr, "Error: Invalid configuration\n");
        return NULL;
    }

    modbus_device_t *device = (modbus_device_t *)malloc(sizeof(modbus_device_t));
    if (!device) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    memcpy(&device->config, config, sizeof(modbus_config_t));
    device->connected = 0;
    device->last_read = 0;

    /* Create Modbus context based on configuration */
    if (config->device) {
        /* RTU mode */
        device->ctx = modbus_new_rtu(
            config->device,
            config->baudrate,
            config->parity,
            config->data_bits,
            config->stop_bits
        );
    } else if (config->host && config->port > 0) {
        /* TCP mode */
        device->ctx = modbus_new_tcp(config->host, config->port);
    } else {
        fprintf(stderr, "Error: Invalid Modbus configuration\n");
        free(device);
        return NULL;
    }

    if (!device->ctx) {
        fprintf(stderr, "Error: Failed to create Modbus context\n");
        free(device);
        return NULL;
    }

    /* Set response timeout */
    struct timeval timeout;
    timeout.tv_sec = MODBUS_READ_TIMEOUT_SEC;
    timeout.tv_usec = MODBUS_READ_TIMEOUT_USEC;
    modbus_set_response_timeout(device->ctx, timeout.tv_sec, timeout.tv_usec);

    return device;
}

/**
 * Connect to Modbus device
 */
int modbus_master_connect(modbus_device_t *device)
{
    if (!device || !device->ctx) {
        fprintf(stderr, "Error: Invalid device\n");
        return -1;
    }

    if (modbus_connect(device->ctx) == -1) {
        fprintf(stderr, "Error: Connection failed - %s\n", modbus_strerror(errno));
        return -1;
    }

    device->connected = 1;
    printf("Modbus connection established\n");
    return 0;
}

/**
 * Disconnect from Modbus device
 */
int modbus_master_disconnect(modbus_device_t *device)
{
    if (!device || !device->ctx) {
        fprintf(stderr, "Error: Invalid device\n");
        return -1;
    }

    if (device->connected) {
        modbus_close(device->ctx);
        device->connected = 0;
        printf("Modbus connection closed\n");
    }

    return 0;
}

/**
 * Read coils from Modbus device
 */
int modbus_master_read_coils(modbus_device_t *device, int address, int count, uint8_t *values)
{
    if (!device || !device->ctx || !device->connected || !values) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }

    int result = modbus_read_bits(device->ctx, address, count, values);
    if (result == -1) {
        fprintf(stderr, "Error: Failed to read coils - %s\n", modbus_strerror(errno));
        return -1;
    }

    device->last_read = time(NULL);
    return result;
}

/**
 * Read discrete inputs from Modbus device
 */
int modbus_master_read_discrete_inputs(modbus_device_t *device, int address, int count, uint8_t *values)
{
    if (!device || !device->ctx || !device->connected || !values) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }

    int result = modbus_read_input_bits(device->ctx, address, count, values);
    if (result == -1) {
        fprintf(stderr, "Error: Failed to read discrete inputs - %s\n", modbus_strerror(errno));
        return -1;
    }

    device->last_read = time(NULL);
    return result;
}

/**
 * Read holding registers from Modbus device
 */
int modbus_master_read_holding_registers(modbus_device_t *device, int address, int count, uint16_t *values)
{
    if (!device || !device->ctx || !device->connected || !values) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }

    int result = modbus_read_registers(device->ctx, address, count, values);
    if (result == -1) {
        fprintf(stderr, "Error: Failed to read holding registers - %s\n", modbus_strerror(errno));
        return -1;
    }

    device->last_read = time(NULL);
    return result;
}

/**
 * Read input registers from Modbus device
 */
int modbus_master_read_input_registers(modbus_device_t *device, int address, int count, uint16_t *values)
{
    if (!device || !device->ctx || !device->connected || !values) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }

    int result = modbus_read_input_registers(device->ctx, address, count, values);
    if (result == -1) {
        fprintf(stderr, "Error: Failed to read input registers - %s\n", modbus_strerror(errno));
        return -1;
    }

    device->last_read = time(NULL);
    return result;
}

/**
 * Write single coil to Modbus device
 */
int modbus_master_write_coil(modbus_device_t *device, int address, int value)
{
    if (!device || !device->ctx || !device->connected) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }

    int result = modbus_write_bit(device->ctx, address, value);
    if (result == -1) {
        fprintf(stderr, "Error: Failed to write coil - %s\n", modbus_strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * Write single register to Modbus device
 */
int modbus_master_write_register(modbus_device_t *device, int address, uint16_t value)
{
    if (!device || !device->ctx || !device->connected) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }

    int result = modbus_write_register(device->ctx, address, value);
    if (result == -1) {
        fprintf(stderr, "Error: Failed to write register - %s\n", modbus_strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * Free Modbus device resources
 */
void modbus_master_free(modbus_device_t *device)
{
    if (!device) {
        return;
    }

    modbus_master_disconnect(device);

    if (device->ctx) {
        modbus_free(device->ctx);
        device->ctx = NULL;
    }

    free(device);
}
