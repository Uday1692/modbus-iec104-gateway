/**
 * @file modbus_master.h
 * @brief Modbus Master Interface
 * 
 * This module provides functionality to read data from Modbus RTU/TCP devices.
 */

#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#include <stdint.h>
#include <time.h>

typedef struct {
    char *host;
    int port;
    char *device;
    int baudrate;
    int parity;
    int data_bits;
    int stop_bits;
} modbus_config_t;

typedef struct modbus_device {
    void *ctx;
    modbus_config_t config;
    int connected;
    time_t last_read;
} modbus_device_t;

typedef struct {
    uint16_t *coils;
    uint16_t *discrete_inputs;
    uint16_t *holding_registers;
    uint16_t *input_registers;
} modbus_data_t;

/**
 * Initialize Modbus master device
 * @param config Configuration structure
 * @return Pointer to modbus_device_t or NULL on error
 */
modbus_device_t *modbus_master_init(modbus_config_t *config);

/**
 * Connect to Modbus device
 * @param device Modbus device handle
 * @return 0 on success, -1 on error
 */
int modbus_master_connect(modbus_device_t *device);

/**
 * Disconnect from Modbus device
 * @param device Modbus device handle
 * @return 0 on success, -1 on error
 */
int modbus_master_disconnect(modbus_device_t *device);

/**
 * Read coils from Modbus device
 * @param device Modbus device handle
 * @param address Starting address
 * @param count Number of coils to read
 * @param values Buffer to store values
 * @return Number of coils read or -1 on error
 */
int modbus_master_read_coils(modbus_device_t *device, int address, int count, uint8_t *values);

/**
 * Read discrete inputs from Modbus device
 * @param device Modbus device handle
 * @param address Starting address
 * @param count Number of inputs to read
 * @param values Buffer to store values
 * @return Number of inputs read or -1 on error
 */
int modbus_master_read_discrete_inputs(modbus_device_t *device, int address, int count, uint8_t *values);

/**
 * Read holding registers from Modbus device
 * @param device Modbus device handle
 * @param address Starting address
 * @param count Number of registers to read
 * @param values Buffer to store values
 * @return Number of registers read or -1 on error
 */
int modbus_master_read_holding_registers(modbus_device_t *device, int address, int count, uint16_t *values);

/**
 * Read input registers from Modbus device
 * @param device Modbus device handle
 * @param address Starting address
 * @param count Number of registers to read
 * @param values Buffer to store values
 * @return Number of registers read or -1 on error
 */
int modbus_master_read_input_registers(modbus_device_t *device, int address, int count, uint16_t *values);

/**
 * Write single coil to Modbus device
 * @param device Modbus device handle
 * @param address Register address
 * @param value Value to write
 * @return 0 on success, -1 on error
 */
int modbus_master_write_coil(modbus_device_t *device, int address, int value);

/**
 * Write single register to Modbus device
 * @param device Modbus device handle
 * @param address Register address
 * @param value Value to write
 * @return 0 on success, -1 on error
 */
int modbus_master_write_register(modbus_device_t *device, int address, uint16_t value);

/**
 * Free Modbus device resources
 * @param device Modbus device handle
 */
void modbus_master_free(modbus_device_t *device);

#endif /* MODBUS_MASTER_H */
