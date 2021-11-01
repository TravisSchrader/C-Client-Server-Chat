
#include <stdio.h>
#include <string.h>

#include "proj.h"

int read_stdin(char *buf, int buf_len, int *more) {
    const char *input = fgets(buf, buf_len, stdin);
    char* ret; 

    ret = strchr(input, '\n');
    *more = (ret == NULL);

    return strlen(input);
}


/**
 * Parse 2 byte header control header.
 *
 * @param header: pointer to a 2-byte header buffer
 * @param mt: Result parameter that'll be filled with Message type field value in the header
 * @param code: Result parameter that'll be filled with CODE field value in the header
 * @param unc: Result parameter that'll be filled with UNC field value in the header
 * @param ulen: Result parameter that'll be filled with ULEN field value in the header
 *
 * @return 1 on success, 0 on failure.
 */
int parse_control_header(void *header, uint8_t *mt, uint8_t *code, uint8_t *unc, uint8_t *ulen) {
    // network byte order is big endian, host byte order is little endian

    // cast the void* to a uint16_t* and read the data as one object. 
    uint16_t* castedHeader = header;
    *castedHeader = *castedHeader;
    uint16_t headerValue = *castedHeader;

    

    
    *ulen = (uint8_t)(headerValue & (uint16_t)15); // 01
    headerValue = headerValue >> 4; // shift 1 for the MT
    
    *unc = (uint8_t)(headerValue &(uint16_t)15);
    headerValue = headerValue >> 7; // shift 7, 4 for the CODE portion and 3 for the reserved bits
    
    *code = (uint8_t)(headerValue & (uint16_t)15); // 1111
    headerValue = headerValue >> 4;  // shifted 4 to move past the unc
   
    *mt = (uint8_t)(headerValue & (uint16_t)1);
    headerValue = 0;

    
    return 1;
}

/**
 * Create a control message header
 *
 * @param header: Pointer to an allocated 2-byte header buffer. This will be filled with the header bytes.
 * @param mt: Message type field value
 * @param code: CODE field value
 * @param unc: UNC field value
 * @param ulen: Username length field value
 *
 * @return 1 on success, 0 on failure.
 */
int create_control_header(void *header, uint8_t mt, uint8_t code, uint8_t unc, uint8_t ulen) {

    uint16_t* castedHeader = header;

    // this is backwards order i think
    uint16_t temp = 0;
    uint16_t ulen16 = ulen;
    uint16_t unc16 = unc;
    uint16_t code16 = code;
    uint16_t mt16 = mt;

    
    temp = temp | ulen16; 
    
    unc16 = unc16 << 4;
    
    
    temp = temp | unc16;
   
    code16 = code16 << 11;
    
    temp = temp | code16;
    
    mt16 = mt16 << 15;
    
    temp = temp | mt16;
    
    *castedHeader = temp;

    header = castedHeader;

    return 0;
}


/**
 * Parse 4 byte header chat message header and fill arguments with the values.
 * @return 1 on success, 0 on failure.
 */
int parse_chat_header(void *header, uint8_t *mt, uint8_t *pub, uint8_t *prv, uint8_t *frg, uint8_t *lst, uint8_t *ulen, uint16_t *length) {
    // cast the void* to a uint16_t* and read the data as one object. 
    uint32_t* castedHeader = header;
    *castedHeader = *castedHeader;
    uint32_t headerValue = *castedHeader;

    *length = (uint16_t)(headerValue & (uint32_t)4095);
    headerValue = headerValue >> 12;

    *ulen = (uint8_t)(headerValue & (uint32_t)15); 
    headerValue = headerValue >> 15; 

    *lst = (uint8_t)(headerValue & (uint32_t)1);
    headerValue = headerValue >> 1; 

    *frg = (uint8_t)(headerValue & (uint32_t)1);
    headerValue = headerValue >> 1; 

    *prv = (uint8_t)(headerValue & (uint32_t)1);
    headerValue = headerValue >> 1; 
    
    *pub = (uint8_t)(headerValue & (uint32_t)1);
    headerValue = headerValue >> 1; 

    *mt = (uint8_t)(headerValue & (uint32_t)1);
    
    headerValue = 0;

    return 1;
}

/**
 * Create a control message header
 *
 * @return 1 on success, 0 on failure.
 */
int create_chat_header(void *header, uint8_t mt, uint8_t pub, uint8_t prv, uint8_t frg, uint8_t lst, uint8_t ulen, uint16_t length) {
uint32_t* castedHeader = header;

    // this is backwards order i think
    uint32_t temp = 0;
    uint32_t length32 = length;
    uint32_t ulen32 = ulen;
    uint32_t lst32 = lst;
    uint32_t frg32 = frg;
    uint32_t prv32 = prv;
    uint32_t pub32 = pub;
    uint32_t mt32 = mt;

   
    temp = temp | length32; 
    
    ulen32 = ulen32 << 12;
    
    temp = temp | ulen32;
    
    lst32 = lst32 << 27;
   
    temp = temp | lst32;
    
    frg32 = frg32 << 28;
    
    temp = temp | frg32;

    prv32 = prv32 << 29;

    temp = temp | prv32;

    pub32 = pub32 << 30;

    temp = temp | pub32;

    mt32 = mt32 << 31;

    temp = temp | mt32;
    
    *castedHeader = temp;

    header = castedHeader;

    return 0;
}
