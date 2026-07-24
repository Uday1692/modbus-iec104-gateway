/**
 * @file iec104_protocol.c
 * @brief IEC 60870-5-104 Protocol Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "iec104_protocol.h"

/**
 * Encode APDU header with I-format control fields
 * Bit layout: I-format (bit 7=0, bit 6=0)
 * Bits 0-14: Send sequence number (Ns)
 * Bits 16-30: Receive sequence number (Nr)
 */
int iec104_encode_apdu_header(uint8_t *buffer, int asdu_len, uint16_t send_seq, uint16_t recv_seq)
{
    if (!buffer) return -1;
    if (asdu_len > IEC104_MAX_ASDU_LEN) return -1;

    /* Start byte */
    buffer[0] = IEC104_APCI_START;
    
    /* APDU length (ASDU length + ASDU header length) */
    buffer[1] = asdu_len + 4;  /* 4 bytes for control field */
    
    /* I-format control field: bit 0 of byte 2-3 is 0 (I-format) */
    /* Bytes 2-3: Send sequence number (Ns) in bits 1-15 */
    uint16_t ns = (send_seq & 0x7FFF) << 1;
    buffer[2] = (ns & 0xFF);
    buffer[3] = ((ns >> 8) & 0xFF);
    
    /* Bytes 4-5: Receive sequence number (Nr) in bits 1-15 */
    uint16_t nr = (recv_seq & 0x7FFF) << 1;
    buffer[4] = (nr & 0xFF);
    buffer[5] = ((nr >> 8) & 0xFF);
    
    return 6; /* Header size */
}

/**
 * Encode S-format (Supervisory) APDU
 * Used for acknowledgment only
 */
int iec104_encode_s_apdu(uint8_t *buffer, uint16_t recv_seq)
{
    if (!buffer) return -1;
    
    buffer[0] = IEC104_APCI_START;
    buffer[1] = 4;  /* APDU length */
    buffer[2] = 0x01;  /* S-format: bits 1,0 = 01 */
    buffer[3] = 0x00;
    
    /* Receive sequence in bits 1-15 of bytes 4-5 */
    uint16_t nr = (recv_seq & 0x7FFF) << 1;
    buffer[4] = (nr & 0xFF);
    buffer[5] = ((nr >> 8) & 0xFF);
    
    return 6;
}

/**
 * Encode U-format (Unnumbered) APDU
 * Used for connection management (STARTDT, STOPDT, TESTFR)
 */
int iec104_encode_u_apdu(uint8_t *buffer, uint8_t function)
{
    if (!buffer) return -1;
    if (function > 0xFF) return -1;
    
    buffer[0] = IEC104_APCI_START;
    buffer[1] = 4;  /* APDU length */
    buffer[2] = function;  /* U-format function in bits 4-7, bits 1,0 = 11 */
    buffer[3] = 0x00;
    buffer[4] = 0x00;
    buffer[5] = 0x00;
    
    return 6;
}

/**
 * Encode Single Point Information (M_SP_NA_1)
 */
int iec104_encode_spi(uint8_t *buffer, uint16_t send_seq, uint16_t recv_seq,
                      uint32_t ioa, uint8_t value, uint8_t quality,
                      uint16_t common_address)
{
    if (!buffer) return -1;
    
    int offset = 0;
    
    /* APDU header (will be filled later) */
    offset += 6;
    
    /* ASDU header */
    buffer[offset++] = IEC104_M_SP_NA_1;  /* Type ID */
    buffer[offset++] = 0x01;               /* SQ=0, Number of elements=1 */
    buffer[offset++] = IEC104_COT_SPONTANEOUS;  /* Cause of Transmission */
    buffer[offset++] = 0x00;               /* Originator */
    buffer[offset++] = (common_address & 0xFF);
    buffer[offset++] = ((common_address >> 8) & 0xFF);
    
    /* Information Object Address (IOA) - 3 bytes */
    buffer[offset++] = (ioa & 0xFF);
    buffer[offset++] = ((ioa >> 8) & 0xFF);
    buffer[offset++] = ((ioa >> 16) & 0xFF);
    
    /* Single Point Information with Quality Descriptor */
    buffer[offset++] = (value & 0x01) | quality;
    
    int asdu_len = offset - 6;
    
    /* Now encode APDU header with correct length */
    iec104_encode_apdu_header(buffer, asdu_len, send_seq, recv_seq);
    
    return offset;
}

/**
 * Encode Measured Value Normalized (M_ME_NA_1)
 */
int iec104_encode_me_na_1(uint8_t *buffer, uint16_t send_seq, uint16_t recv_seq,
                          uint32_t ioa, uint16_t value, uint8_t quality,
                          uint16_t common_address)
{
    if (!buffer) return -1;
    
    int offset = 0;
    
    /* APDU header */
    offset += 6;
    
    /* ASDU header */
    buffer[offset++] = IEC104_M_ME_NA_1;  /* Type ID */
    buffer[offset++] = 0x01;               /* SQ=0, Number of elements=1 */
    buffer[offset++] = IEC104_COT_SPONTANEOUS;
    buffer[offset++] = 0x00;
    buffer[offset++] = (common_address & 0xFF);
    buffer[offset++] = ((common_address >> 8) & 0xFF);
    
    /* Information Object Address */
    buffer[offset++] = (ioa & 0xFF);
    buffer[offset++] = ((ioa >> 8) & 0xFF);
    buffer[offset++] = ((ioa >> 16) & 0xFF);
    
    /* Measured value (16-bit normalized) */
    buffer[offset++] = (value & 0xFF);
    buffer[offset++] = ((value >> 8) & 0xFF);
    
    /* Quality descriptor */
    buffer[offset++] = quality;
    
    int asdu_len = offset - 6;
    iec104_encode_apdu_header(buffer, asdu_len, send_seq, recv_seq);
    
    return offset;
}

/**
 * Encode Measured Value Floating Point (M_ME_TF_1)
 */
int iec104_encode_me_tf_1(uint8_t *buffer, uint16_t send_seq, uint16_t recv_seq,
                          uint32_t ioa, float value, uint8_t quality,
                          uint16_t common_address)
{
    if (!buffer) return -1;
    
    int offset = 0;
    
    /* APDU header */
    offset += 6;
    
    /* ASDU header */
    buffer[offset++] = IEC104_M_ME_TF_1;  /* Type ID */
    buffer[offset++] = 0x01;              /* SQ=0, Number of elements=1 */
    buffer[offset++] = IEC104_COT_SPONTANEOUS;
    buffer[offset++] = 0x00;
    buffer[offset++] = (common_address & 0xFF);
    buffer[offset++] = ((common_address >> 8) & 0xFF);
    
    /* Information Object Address */
    buffer[offset++] = (ioa & 0xFF);
    buffer[offset++] = ((ioa >> 8) & 0xFF);
    buffer[offset++] = ((ioa >> 16) & 0xFF);
    
    /* Measured value (32-bit float) */
    uint32_t *fvalue = (uint32_t *)&value;
    buffer[offset++] = (*fvalue & 0xFF);
    buffer[offset++] = ((*fvalue >> 8) & 0xFF);
    buffer[offset++] = ((*fvalue >> 16) & 0xFF);
    buffer[offset++] = ((*fvalue >> 24) & 0xFF);
    
    /* Quality descriptor */
    buffer[offset++] = quality;
    
    int asdu_len = offset - 6;
    iec104_encode_apdu_header(buffer, asdu_len, send_seq, recv_seq);
    
    return offset;
}

/**
 * Decode APDU and extract sequence numbers
 */
int iec104_decode_apdu(const uint8_t *buffer, int length, uint16_t *send_seq, uint16_t *recv_seq)
{
    if (!buffer || length < 6) return -1;
    if (buffer[0] != IEC104_APCI_START) return -1;
    
    int apdu_len = buffer[1];
    if (length < (apdu_len + 2)) return -1;
    
    int apdu_type = iec104_get_apdu_type(buffer);
    
    if (apdu_type == IEC104_APDU_I) {
        /* I-format: extract Ns and Nr from bytes 2-5 */
        uint16_t ns_raw = (buffer[3] << 8) | buffer[2];
        uint16_t nr_raw = (buffer[5] << 8) | buffer[4];
        
        if (send_seq) *send_seq = (ns_raw >> 1) & 0x7FFF;
        if (recv_seq) *recv_seq = (nr_raw >> 1) & 0x7FFF;
    } else if (apdu_type == IEC104_APDU_S) {
        /* S-format: only Nr is valid */
        uint16_t nr_raw = (buffer[5] << 8) | buffer[4];
        if (recv_seq) *recv_seq = (nr_raw >> 1) & 0x7FFF;
        if (send_seq) *send_seq = 0;
    }
    
    return apdu_type;
}

/**
 * Get APDU type (I, S, or U format)
 * Bits 0-1 of byte 2 determine format
 * 00 = I-format
 * 01 = S-format
 * 11 = U-format
 */
int iec104_get_apdu_type(const uint8_t *buffer)
{
    if (!buffer) return -1;
    
    uint8_t control = buffer[2];
    
    if ((control & 0x03) == 0x00) return IEC104_APDU_I;
    if ((control & 0x03) == 0x01) return IEC104_APDU_S;
    if ((control & 0x03) == 0x03) return IEC104_APDU_U;
    
    return -1;
}

/**
 * Get U-format function code
 */
int iec104_get_u_function(const uint8_t *buffer)
{
    if (!buffer) return -1;
    if (iec104_get_apdu_type(buffer) != IEC104_APDU_U) return -1;
    
    return (buffer[2] & 0xFC);  /* Bits 2-7 contain function */
}
