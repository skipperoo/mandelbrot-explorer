#ifndef _WEBSOCKET_H_
#define _WEBSOCKET_H_
#include <arpa/inet.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Define buffer size for TCP reads
#define BUFFER_SIZE 4096
#define PORT 8080
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
int receive_frame(int client_fd, char *out_payload);
void send_binary_frame(int client_fd, uint8_t *data, size_t len);
int perform_handshake(int client_fd, char *request_buffer);
#endif // _WEBSOCKET_H_
