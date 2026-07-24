/**
 * @file iec104_protocol.h
 * @brief IEC 60870-5-104 Protocol Implementation
 * 
 * Full IEC 104 protocol support with APDU handling
 */

#ifndef IEC104_PROTOCOL_H
#define IEC104_PROTOCOL_H

#include <stdint.h>
#include <time.h>
#include <sys/socket.h>

/* IEC 104 Constants */
#define IEC104_APCI_START 0x68
#define IEC104_DEFAULT_PORT 2404
#define IEC104_TIMEOUT_SEC 30
#define IEC104_MAX_ASDU_LEN 252
#define IEC104_SEND_SEQUENCE_MAX 32768
#define IEC104_RECV_SEQUENCE_MAX 32768
#define IEC104_T0_TIMEOUT 10    /* Connection establishment */
#define IEC104_T1_TIMEOUT 15    /* Sent APDU timeout */
#define IEC104_T2_TIMEOUT 10    /* Idle time before testfr act */
#define IEC104_T3_TIMEOUT 20    /* Test frame send timeout */
#define IEC104_K_SEND 12        /* Max unconfirmed APDUs */
#define IEC104_W_RECV 8         /* Recv seq before confirmation */

/* APDU Types */
#define IEC104_APDU_I 0         /* I-format (information) */
#define IEC104_APDU_S 1         /* S-format (supervisory) */
#define IEC104_APDU_U 3         /* U-format (unnumbered) */

/* U-format control functions */
#define IEC104_STARTDT_ACT 0x07
#define IEC104_STARTDT_CON 0x0B
#define IEC104_STOPDT_ACT 0x13
#define IEC104_STOPDT_CON 0x23
#define IEC104_TESTFR_ACT 0x43
#define IEC104_TESTFR_CON 0x83

/* ASDU Type Identifications (TypeID) */
#define IEC104_M_SP_NA_1 1      /* Single-point information */
#define IEC104_M_SP_TA_1 2      /* Single-point with timestamp */
#define IEC104_M_DP_NA_1 3      /* Double-point information */
#define IEC104_M_DP_TA_1 4      /* Double-point with timestamp */
#define IEC104_M_ST_NA_1 5      /* Step position information */
#define IEC104_M_ME_NA_1 9      /* Measured value (normalized) */
#define IEC104_M_ME_TF_1 11     /* Measured value (floating point) */
#define IEC104_M_IT_NA_1 15     /* Integrated totals */
#define IEC104_M_PS_NA_1 20     /* Packed single point */
#define IEC104_M_ME_ND_1 21     /* Measured value (scaled) */
#define IEC104_M_SP_TB_1 30     /* Single-point with timestamp */
#define IEC104_M_DP_TB_1 31     /* Double-point with timestamp */
#define IEC104_M_ST_TB_1 32     /* Step position with timestamp */
#define IEC104_M_ME_TD_1 34     /* Measured value (floating) with timestamp */
#define IEC104_M_ME_TE_1 35     /* Measured value (scaled) with timestamp */
#define IEC104_M_IT_TB_1 36     /* Integrated totals with timestamp */

/* Qualifier of Information (QoI) */
#define IEC104_QOI_STATION 20   /* Station interrogation */
#define IEC104_QOI_GROUP_1 21   /* Group 1 interrogation */
#define IEC104_QOI_GROUP_16 36  /* Group 16 interrogation */

/* Quality Descriptor */
#define IEC104_QDS_VALID 0x00
#define IEC104_QDS_INVALID 0x80
#define IEC104_QDS_RESERVED 0x40
#define IEC104_QDS_SUBSTITUTED 0x20
#define IEC104_QDS_NOT_TOPICAL 0x10
#define IEC104_QDS_OVERFLOW 0x01

/* Single Point Information states */
#define IEC104_SPI_OFF 0
#define IEC104_SPI_ON 1

/* Cause of Transmission (CoT) */
#define IEC104_COT_PERIODIC 1
#define IEC104_COT_BACKGROUND 2
#define IEC104_COT_SPONTANEOUS 3
#define IEC104_COT_INITIALIZED 4
#define IEC104_COT_REQUEST 5
#define IEC104_COT_ACTIVATION 6
#define IEC104_COT_ACTIVATION_CON 7
#define IEC104_COT_DEACTIVATION 8
#define IEC104_COT_DEACTIVATION_CON 9
#define IEC104_COT_ACTIVATION_TERMINATION 10
#define IEC104_COT_RESPONSE 20
#define IEC104_COT_NEGATIVE 21
#define IEC104_COT_UNKNOWN_COT 22
#define IEC104_COT_UNKNOWN_COMMON_ADDRESS 23
#define IEC104_COT_UNKNOWN_ASDU_ADDRESS 24

/* APDU Structures */
packed_t struct {
    uint8_t start_byte;          /* 0x68 */
    uint8_t length;              /* APDU length (without start and length bytes) */
    uint8_t control_field[4];    /* Control bytes */
} iec104_apdu_header_t;

packed_t struct {
    uint8_t type_id;             /* ASDU Type Identification */
    uint8_t sq_num_elements;     /* SQ (1) + Number of Elements (7) */
    uint8_t cause_tx;            /* Cause of Transmission */
    uint8_t cause_tx_originator; /* Originator address */
    uint16_t common_address;     /* Common Address of ASDU */
} iec104_asdu_header_t;

/* Single Point Information (M_SP_NA_1) */
packed_t struct {
    uint8_t spi;                 /* SPI value (1 bit) + Quality (7 bits) */
} iec104_spi_t;

/* Measured Value Normalized (M_ME_NA_1) */
packed_t struct {
    uint16_t value;              /* 16-bit normalized value */
    uint8_t quality;             /* Quality descriptor */
} iec104_me_na_1_t;

/* Measured Value Floating Point (M_ME_TF_1) */
packed_t struct {
    float value;                 /* 32-bit float */
    uint8_t quality;             /* Quality descriptor */
} iec104_me_tf_1_t;

/* Client connection state */
typedef enum {
    IEC104_CLOSED = 0,
    IEC104_OPENING = 1,
    IEC104_OPEN = 2,
    IEC104_ACTIVE = 3
} iec104_state_t;

/* Client connection */
typedef struct {
    int socket;
    iec104_state_t state;
    uint16_t send_seq;
    uint16_t recv_seq;
    time_t last_activity;
    uint8_t unconfirmed_count;
} iec104_client_t;

/* Encode APDU header */
int iec104_encode_apdu_header(uint8_t *buffer, int length, uint16_t send_seq, uint16_t recv_seq);

/* Encode S-format APDU */
int iec104_encode_s_apdu(uint8_t *buffer, uint16_t recv_seq);

/* Encode U-format APDU */
int iec104_encode_u_apdu(uint8_t *buffer, uint8_t function);

/* Encode I-format APDU with single point */
int iec104_encode_spi(uint8_t *buffer, uint16_t send_seq, uint16_t recv_seq,
                      uint32_t ioa, uint8_t value, uint8_t quality,
                      uint16_t common_address);

/* Encode I-format APDU with measured value */
int iec104_encode_me_na_1(uint8_t *buffer, uint16_t send_seq, uint16_t recv_seq,
                          uint32_t ioa, uint16_t value, uint8_t quality,
                          uint16_t common_address);

/* Encode I-format APDU with floating point */
int iec104_encode_me_tf_1(uint8_t *buffer, uint16_t send_seq, uint16_t recv_seq,
                          uint32_t ioa, float value, uint8_t quality,
                          uint16_t common_address);

/* Decode APDU */
int iec104_decode_apdu(const uint8_t *buffer, int length, uint16_t *send_seq, uint16_t *recv_seq);

/* Get APDU type */
int iec104_get_apdu_type(const uint8_t *buffer);

/* Get U-format function */
int iec104_get_u_function(const uint8_t *buffer);

#endif /* IEC104_PROTOCOL_H */
