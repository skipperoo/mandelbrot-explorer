#include "include/websocket_utils.h"

// --- Base64 Encoding ---
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void base64_encode(const unsigned char *src, size_t len, char *out) {
  int i, j;
  for (i = 0, j = 0; i < len; i += 3) {
    uint32_t v = src[i];
    v = i + 1 < len ? v << 8 | src[i + 1] : v << 8;
    v = i + 2 < len ? v << 8 | src[i + 2] : v << 8;

    out[j++] = base64_table[(v >> 18) & 0x3F];
    out[j++] = base64_table[(v >> 12) & 0x3F];
    if (i + 1 < len)
      out[j++] = base64_table[(v >> 6) & 0x3F];
    else
      out[j++] = '=';
    if (i + 2 < len)
      out[j++] = base64_table[v & 0x3F];
    else
      out[j++] = '=';
  }
  out[j] = '\0';
}

#define ROL(v, b) (((v) << (b)) | ((v) >> (32 - (b))))

void sha1(const unsigned char *data, size_t len, unsigned char *hash) {
  uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476,
           h4 = 0xC3D2E1F0;
  uint32_t w[80];

  // Calculate total length including padding
  // Padding: 1 byte (0x80) + k zero bytes + 8 bytes length
  // Total size must be multiple of 64 bytes
  size_t padded_len = len + 1 + 8;
  size_t num_blocks = (padded_len + 63) / 64;

  for (size_t b = 0; b < num_blocks; b++) {
    unsigned char block[64] = {0};
    size_t start_idx = b * 64;

    // 1. Copy Data
    if (start_idx < len) {
      size_t bytes_to_copy = (len - start_idx >= 64) ? 64 : (len - start_idx);
      memcpy(block, data + start_idx, bytes_to_copy);
    }

    // 2. Append 0x80 (only in the block where data ends)
    if (start_idx <= len && start_idx + 64 > len) {
      block[len - start_idx] = 0x80;
    }

    // 3. Append Length (only in the very last block)
    if (b == num_blocks - 1) {
      uint64_t bit_len = (uint64_t)len * 8;
      for (int k = 0; k < 8; k++) {
        block[63 - k] = (bit_len >> (k * 8)) & 0xFF;
      }
    }

    // Process Block
    for (int j = 0; j < 16; j++)
      w[j] = (block[j * 4] << 24) | (block[j * 4 + 1] << 16) |
             (block[j * 4 + 2] << 8) | (block[j * 4 + 3]);

    for (int j = 16; j < 80; j++)
      w[j] = ROL(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);

    uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;

    for (int j = 0; j < 80; j++) {
      uint32_t f, k;
      if (j < 20) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      } else if (j < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (j < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }

      uint32_t temp = ROL(a, 5) + f + e + k + w[j];
      e = d;
      d = c;
      c = ROL(b, 30);
      b = a;
      a = temp;
    }

    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
  }

  // Write Hash (Big Endian)
  for (int i = 0; i < 4; i++)
    hash[i] = (h0 >> (24 - i * 8)) & 0xFF;
  for (int i = 0; i < 4; i++)
    hash[i + 4] = (h1 >> (24 - i * 8)) & 0xFF;
  for (int i = 0; i < 4; i++)
    hash[i + 8] = (h2 >> (24 - i * 8)) & 0xFF;
  for (int i = 0; i < 4; i++)
    hash[i + 12] = (h3 >> (24 - i * 8)) & 0xFF;
  for (int i = 0; i < 4; i++)
    hash[i + 16] = (h4 >> (24 - i * 8)) & 0xFF;
}
