/**
 * @file config_parser.h
 * @brief Simple INI-style configuration parser
 */

#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <stdint.h>
#include "modbus_master.h"

#define CFG_MAX_MAPPINGS 256

typedef enum {
    MAP_BOOLEAN,
    MAP_INT16,
    MAP_UINT16,
    MAP_INT32,
    MAP_UINT32,
    MAP_FLOAT,
} mapping_type_t;

typedef struct {
    int modbus_address;
    int modbus_function;        /* 1=coil, 2=discrete, 3=holding, 4=input */
    mapping_type_t data_type;
    uint32_t iec104_ioa;
    int read_interval_ms;
    float scale;
    float offset;
} data_mapping_t;

typedef struct {
    /* logging */
    char log_level[16];
    char log_file[256];

    /* modbus */
    modbus_config_t modbus;
    int modbus_slave_id;
    int modbus_timeout;
    int modbus_poll_interval_ms;

    /* iec104 */
    char iec104_bind_address[64];
    int iec104_port;
    int iec104_max_clients;
    int iec104_timeout;
    int iec104_common_address;

    /* mappings */
    data_mapping_t mappings[CFG_MAX_MAPPINGS];
    int mapping_count;
} gateway_config_t;

/**
 * Parse a configuration file into gateway_config_t.
 * @param filename Path to config file
 * @param config Output structure
 * @return 0 on success, -1 on error
 */
int parse_config_file(const char *filename, gateway_config_t *config);

/**
 * Convert a data-type string to mapping_type_t.
 */
mapping_type_t parse_mapping_type(const char *str);

/**
 * Convert a mapping_type_t to iec104_data_type_t.
 */
int mapping_to_iec104_type(mapping_type_t type);

/**
 * Convert mapping modbus function code to a string for logging.
 */
const char *modbus_function_name(int function);

#endif /* CONFIG_PARSER_H */
