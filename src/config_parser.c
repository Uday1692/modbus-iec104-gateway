/**
 * @file config_parser.c
 * @brief Simple INI-style configuration parser for the gateway
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config_parser.h"
#include "iec104_server.h"

#define MAX_LINE 512

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

static int starts_with(const char *line, const char *prefix)
{
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

static const char *skip_to_value(const char *line)
{
    const char *eq = strchr(line, '=');
    if (!eq) return NULL;
    eq++;
    while (*eq && isspace((unsigned char)*eq)) eq++;
    return eq;
}

static void copy_value(const char *src, char *dst, size_t dst_size)
{
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
    while (len > 0 && isspace((unsigned char)dst[len - 1])) {
        dst[len - 1] = '\0';
        len--;
    }
}

static int parse_parity_char(const char *s)
{
    if (!s || !*s) return 'N';
    if (strcasecmp(s, "none") == 0) return 'N';
    if (strcasecmp(s, "even") == 0) return 'E';
    if (strcasecmp(s, "odd") == 0) return 'O';
    return s[0];
}

static int parse_modbus_function_for_type(mapping_type_t type, int address)
{
    /* Pick sensible defaults based on data type.
     * Override by user is not yet supported in the file format. */
    switch (type) {
    case MAP_BOOLEAN:
        return (address < 10000) ? 1 : 2;  /* coil if address is small, else DI */
    case MAP_INT16:
    case MAP_UINT16:
    case MAP_INT32:
    case MAP_UINT32:
    case MAP_FLOAT:
        return (address < 30000) ? 3 : 4;  /* holding register if small, else input */
    }
    return 3;
}

mapping_type_t parse_mapping_type(const char *str)
{
    if (!str) return MAP_UINT16;
    if (strcasecmp(str, "boolean") == 0) return MAP_BOOLEAN;
    if (strcasecmp(str, "bool") == 0) return MAP_BOOLEAN;
    if (strcasecmp(str, "int16") == 0) return MAP_INT16;
    if (strcasecmp(str, "uint16") == 0) return MAP_UINT16;
    if (strcasecmp(str, "int32") == 0) return MAP_INT32;
    if (strcasecmp(str, "uint32") == 0) return MAP_UINT32;
    if (strcasecmp(str, "float") == 0) return MAP_FLOAT;
    return MAP_UINT16;
}

int mapping_to_iec104_type(mapping_type_t type)
{
    switch (type) {
    case MAP_BOOLEAN:   return IEC104_DT_BOOLEAN;
    case MAP_INT16:     return IEC104_DT_INT32;  /* stored as 32-bit internally */
    case MAP_UINT16:    return IEC104_DT_UINT16;
    case MAP_INT32:     return IEC104_DT_INT32;
    case MAP_UINT32:    return IEC104_DT_INT32;  /* lossy, but fits int32 */
    case MAP_FLOAT:     return IEC104_DT_FLOAT;
    }
    return IEC104_DT_UINT16;
}

const char *modbus_function_name(int function)
{
    switch (function) {
    case 1: return "coil";
    case 2: return "discrete_input";
    case 3: return "holding_register";
    case 4: return "input_register";
    default: return "unknown";
    }
}

static int parse_mapping_line(const char *line, data_mapping_t *mapping)
{
    /* Format: modbus_address:iec104_ioa:data_type:read_interval[:modbus_function][:scale][:offset] */
    char buf[256];
    copy_value(line, buf, sizeof(buf));

    char *parts[8] = {0};
    int count = 0;
    char *token = strtok(buf, ":");
    while (token && count < 8) {
        parts[count++] = token;
        token = strtok(NULL, ":");
    }

    if (count < 4) return -1;

    mapping->modbus_address = atoi(parts[0]);
    mapping->iec104_ioa = (uint32_t)atoi(parts[1]);
    mapping->data_type = parse_mapping_type(parts[2]);
    mapping->read_interval_ms = atoi(parts[3]);

    /* Optional fields: modbus_function, scale, offset.
     * If the 5th token is a single digit 1-4, treat it as the function code. */
    int fn_index = -1;
    int scale_index = -1;
    int offset_index = -1;

    if (count > 4 && parts[4][0] && strlen(parts[4]) == 1 && parts[4][0] >= '1' && parts[4][0] <= '4') {
        fn_index = 4;
        scale_index = (count > 5) ? 5 : -1;
        offset_index = (count > 6) ? 6 : -1;
    } else {
        scale_index = (count > 4) ? 4 : -1;
        offset_index = (count > 5) ? 5 : -1;
    }

    mapping->modbus_function = (fn_index >= 0)
        ? atoi(parts[fn_index])
        : parse_modbus_function_for_type(mapping->data_type, mapping->modbus_address);

    mapping->scale = (scale_index >= 0 && parts[scale_index][0])
        ? atof(parts[scale_index]) : 1.0f;
    mapping->offset = (offset_index >= 0 && parts[offset_index][0])
        ? atof(parts[offset_index]) : 0.0f;

    return 0;
}

int parse_config_file(const char *filename, gateway_config_t *config)
{
    if (!filename || !config) return -1;

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open config file %s\n", filename);
        return -1;
    }

    memset(config, 0, sizeof(gateway_config_t));

    /* Defaults */
    strcpy(config->log_level, "INFO");
    strcpy(config->log_file, "/var/log/gateway.log");

    strcpy(config->modbus.host, "127.0.0.1");
    config->modbus.port = 502;
    config->modbus.parity = 'N';
    config->modbus.data_bits = 8;
    config->modbus.stop_bits = 1;
    config->modbus.slave_id = 1;
    config->modbus_slave_id = 1;
    config->modbus_timeout = 5;
    config->modbus_poll_interval_ms = 1000;

    strcpy(config->iec104_bind_address, "0.0.0.0");
    config->iec104_port = 2404;
    config->iec104_max_clients = 10;
    config->iec104_timeout = 30;
    config->iec104_common_address = 1;

    char raw[MAX_LINE];
    char section[64] = "";
    int modbus_is_rtu = 0;

    while (fgets(raw, sizeof(raw), fp)) {
        char *line = trim(raw);
        if (*line == '\0' || *line == '#') continue;

        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(section, line + 1, sizeof(section) - 1);
                section[sizeof(section) - 1] = '\0';
            }
            continue;
        }

        const char *value = skip_to_value(line);

        if (strcmp(section, "modbus") == 0) {
            if (starts_with(line, "type=")) {
                modbus_is_rtu = (value && strcasecmp(value, "rtu") == 0);
            } else if (starts_with(line, "host=")) {
                copy_value(value, config->modbus.host, sizeof(config->modbus.host));
            } else if (starts_with(line, "port=")) {
                config->modbus.port = atoi(value);
            } else if (starts_with(line, "device=")) {
                copy_value(value, config->modbus.device, sizeof(config->modbus.device));
            } else if (starts_with(line, "baudrate=")) {
                config->modbus.baudrate = atoi(value);
            } else if (starts_with(line, "parity=")) {
                config->modbus.parity = parse_parity_char(value);
            } else if (starts_with(line, "data_bits=")) {
                config->modbus.data_bits = atoi(value);
            } else if (starts_with(line, "stop_bits=")) {
                config->modbus.stop_bits = atoi(value);
            } else if (starts_with(line, "slave_id=")) {
                config->modbus.slave_id = atoi(value);
                config->modbus_slave_id = config->modbus.slave_id;
            } else if (starts_with(line, "timeout=")) {
                config->modbus_timeout = atoi(value);
            } else if (starts_with(line, "poll_interval=")) {
                config->modbus_poll_interval_ms = atoi(value);
            }
        } else if (strcmp(section, "iec104") == 0) {
            if (starts_with(line, "bind_address=")) {
                copy_value(value, config->iec104_bind_address, sizeof(config->iec104_bind_address));
            } else if (starts_with(line, "port=")) {
                config->iec104_port = atoi(value);
            } else if (starts_with(line, "max_clients=")) {
                config->iec104_max_clients = atoi(value);
            } else if (starts_with(line, "timeout=")) {
                config->iec104_timeout = atoi(value);
            } else if (starts_with(line, "common_address=")) {
                config->iec104_common_address = atoi(value);
            }
        } else if (strcmp(section, "mapping") == 0) {
            if (config->mapping_count >= CFG_MAX_MAPPINGS) continue;
            if (parse_mapping_line(line, &config->mappings[config->mapping_count]) == 0) {
                config->mapping_count++;
            } else {
                fprintf(stderr, "Warning: Invalid mapping line: %s\n", line);
            }
        } else if (strcmp(section, "logging") == 0 || section[0] == '\0') {
            if (starts_with(line, "log_level=")) {
                copy_value(value, config->log_level, sizeof(config->log_level));
            } else if (starts_with(line, "log_file=")) {
                copy_value(value, config->log_file, sizeof(config->log_file));
            }
        }
    }

    fclose(fp);

    /* If type=rtu but device is empty, blank the host so init fails cleanly. */
    if (modbus_is_rtu && config->modbus.device[0] == '\0') {
        config->modbus.host[0] = '\0';
        config->modbus.port = 0;
    }

    return 0;
}
