#include "include/websocket.h"
#include "include/websocket_utils.h"

int perform_handshake(int client_fd, char *request_buffer) {
  char *key_start = strstr(request_buffer, "Sec-WebSocket-Key: ");
  if (!key_start)
    return -1;

  key_start += 19;
  char *key_end = strstr(key_start, "\r\n");
  if (!key_end)
    return -1;

  char key[64];
  int key_len = key_end - key_start;
  strncpy(key, key_start, key_len);
  key[key_len] = '\0';

  // Concatenate Key + GUID
  char input[128];
  sprintf(input, "%s%s", key, WS_GUID);

  // Hash and Encode
  unsigned char hash[20];
  sha1((unsigned char *)input, strlen(input), hash);

  char accept_key[32];
  base64_encode(hash, 20, accept_key);

  // Send Response
  char response[256];
  sprintf(response,
          "HTTP/1.1 101 Switching Protocols\r\n"
          "Upgrade: websocket\r\n"
          "Connection: Upgrade\r\n"
          "Sec-WebSocket-Accept: %s\r\n\r\n",
          accept_key);

  send(client_fd, response, strlen(response), 0);
  return 0;
}

// --- 2. Frame Sending (Server -> Client) ---
// We only implement sending BINARY frames (opcode 0x2) for images
void send_binary_frame(int client_fd, uint8_t *data, size_t len) {
  uint8_t header[10];
  int header_len = 0;

  header[0] = 0x82; // FIN bit set (1) | Binary Opcode (0x2) => 10000010

  if (len <= 125) {
    header[1] = len; // No Mask bit sent from server
    header_len = 2;
  } else if (len <= 65535) {
    header[1] = 126;
    header[2] = (len >> 8) & 0xFF;
    header[3] = len & 0xFF;
    header_len = 4;
  } else {
    header[1] = 127;
    // 64-bit length (assuming size_t fits in 64 bit and length isn't massive)
    for (int i = 0; i < 8; i++) {
      header[2 + i] = (len >> ((7 - i) * 8)) & 0xFF;
    }
    header_len = 10;
  }

  // Send header then data
  send(client_fd, header, header_len, 0);
  send(client_fd, data, len, 0);
}

// --- 3. Frame Receiving (Client -> Server) ---
// Returns 1 if valid text frame received, 0 on close/error
// 'out_payload' buffer must be large enough
int receive_frame(int client_fd, char *out_payload) {
  uint8_t header[2];
  if (recv(client_fd, header, 2, 0) <= 0)
    return 0;

  // Helper to check Opcode (we want Text 0x1 for coordinates)
  int opcode = header[0] & 0x0F;
  if (opcode == 0x8)
    return 0; // Close frame

  uint8_t len_indicator = header[1] & 0x7F;
  int is_masked = (header[1] & 0x80); // Client must mask

  uint64_t payload_len = 0;
  if (len_indicator <= 125) {
    payload_len = len_indicator;
  } else if (len_indicator == 126) {
    uint8_t ext_len[2];
    recv(client_fd, ext_len, 2, 0);
    payload_len = (ext_len[0] << 8) | ext_len[1];
  }
  // Usually requests for coords are small so we skip 127 logic for simplicity

  uint8_t mask_key[4];
  if (is_masked) {
    recv(client_fd, mask_key, 4, 0);
  }

  uint8_t *buffer = malloc(payload_len + 1);
  size_t total_read = 0;
  while (total_read < payload_len) {
    total_read +=
        recv(client_fd, buffer + total_read, payload_len - total_read, 0);
  }

  // Unmask
  if (is_masked) {
    for (size_t i = 0; i < payload_len; i++) {
      out_payload[i] = buffer[i] ^ mask_key[i % 4];
    }
  } else {
    memcpy(out_payload, buffer, payload_len);
  }
  out_payload[payload_len] = '\0'; // Null terminate string
  free(buffer);

  return 1;
}
