#ifndef _WEBSOCKET_UTILS_H_
#define _WEBSOCKET_UTILS_H_
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void base64_encode(const unsigned char *src, size_t len, char *out);
void sha1(const unsigned char *data, size_t len, unsigned char *hash);

#endif // _WEBSOCKET_UTILS_H_
