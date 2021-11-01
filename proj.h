
#include <stdint.h>

/*
 * Do not modify the following lines. Add your code at the end of this file.
 */
#ifdef DEBUG_FLAG
#define DEBUG_wARG(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#define DEBUG_MSG(msg) fprintf(stderr, "%s\n", msg);
#else
#define DEBUG_wARG(fmt, ...)
#define DEBUG_MSG(msg)
#endif

#define PRINT_wARG(fmt, ...) printf(fmt, __VA_ARGS__); fflush(stdout)
#define PRINT_MSG(msg) printf("%s", msg); fflush(stdout)


int read_stdin(char *buf, int buf_len, int *more);
int parse_control_header(void *header, uint8_t *mt, uint8_t *code, uint8_t *unc, uint8_t *ulen);
int create_control_header(void *header, uint8_t mt, uint8_t code, uint8_t unc, uint8_t ulen);
int parse_chat_header(void *header, uint8_t *mt, uint8_t *pub, uint8_t *prv, uint8_t *frg, uint8_t *lst, uint8_t *ulen, uint16_t *length);
int create_chat_header(void *header, uint8_t mt, uint8_t pub, uint8_t prv, uint8_t frg, uint8_t lst, uint8_t ulen, uint16_t length);
// Do not modify the above lines. Add your code below.

