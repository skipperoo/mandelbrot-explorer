# Security Analysis Report: Mandelbrot Web Server

This report outlines critical security vulnerabilities, memory management issues, and logic errors identified during a comprehensive audit of the backend C codebase.

---

## 1. Critical: Stack-based Buffer Overflow in Websocket Frame Handling

### Description
In `server.c`, a fixed-size buffer `char payload[1024]` is allocated on the stack. This buffer is passed to `receive_frame`, which reads the `payload_len` directly from the Websocket header (up to 65,535 bytes for 16-bit lengths) and copies data into `payload` without bounds checking.

### Impact
An attacker can send a specially crafted Websocket frame with a large payload, overwriting the stack. This can lead to **Remote Code Execution (RCE)** or a denial-of-service (DoS) crash.

### Vulnerable Code (`backend/src/websocket.c`)
```c
int receive_frame(int client_fd, char *out_payload) {
  // ... (header parsing)
  uint8_t *buffer = malloc(payload_len + 1);
  // ... (recv into buffer)
  if (is_masked) {
    for (size_t i = 0; i < payload_len; i++) {
      out_payload[i] = buffer[i] ^ mask_key[i % 4]; // No bounds check on out_payload
    }
  }
  out_payload[payload_len] = '\0';
}
```

### Recommended Fix
Update the function signature to include the buffer capacity and enforce strict bounds checking.

```c
int receive_frame(int client_fd, char *out_payload, size_t max_len) {
  // ... (header parsing)
  if (payload_len >= max_len) {
      // Handle error: payload too large for buffer
      return -1;
  }
  uint8_t *buffer = malloc(payload_len + 1);
  if (!buffer) return 0;
  // ... (recv and unmask)
  memcpy(out_payload, buffer, payload_len);
  out_payload[payload_len] = '\0';
  free(buffer);
  return 1;
}
```

---

## 2. High: Buffer Overflow in Websocket Handshake

### Description
The `perform_handshake` function extracts the `Sec-WebSocket-Key` using `strncpy` based on the distance between two pointers in the request buffer. If the key provided by the client exceeds 63 characters, it overflows the `key[64]` buffer.

### Impact
Memory corruption on the stack, potentially leading to arbitrary code execution.

### Vulnerable Code (`backend/src/websocket.c`)
```c
char key[64];
int key_len = key_end - key_start;
strncpy(key, key_start, key_len); // Risk: key_len can be > 64
key[key_len] = '\0';
```

### Recommended Fix
Validate the key length before copying.

```c
char key[64];
int key_len = key_end - key_start;
if (key_len >= sizeof(key)) return -1; // Added check
strncpy(key, key_start, key_len);
key[key_len] = '\0';
```

---

## 3. High: Thread Pool Task Overwrite and Memory Leak

### Description
`tpool_add_work` blindly increments `queue_tail` and `count` without checking if the queue is already full. In a high-load scenario, new tasks will overwrite pending tasks in the circular buffer.

### Impact
Leaked memory (if the overwritten `render_job_t` was dynamically allocated) and dropped tasks, leading to inconsistent application state or hangs.

### Vulnerable Code (`backend/src/tpool.c`)
```c
void tpool_add_work(tpool_t *pool, void (*func)(void *), void *arg) {
  pthread_mutex_lock(&pool->lock);
  pool->queue[pool->queue_tail] = (thread_task_t){.function = func, .argument = arg};
  pool->queue_tail = (pool->queue_tail + 1) % pool->queue_size;
  pool->count++; // No check if count > queue_size
  pthread_cond_signal(&pool->notify);
  pthread_mutex_unlock(&pool->lock);
}
```

### Recommended Fix
Implement a blocking wait or return an error when the queue is full.

```c
void tpool_add_work(tpool_t *pool, void (*func)(void *), void *arg) {
  pthread_mutex_lock(&pool->lock);
  while (pool->count == pool->queue_size && !pool->shutdown) {
      // Optional: block until space is available or return error
      pthread_cond_wait(&pool->full_cond, &pool->lock); 
  }
  // ... (insert task)
  pthread_mutex_unlock(&pool->lock);
}
```

---

## 4. Medium: SIMD Memory Corruption (Out-of-bounds Write)

### Description
The SIMD rendering paths (AVX/AVX2) process pixels in blocks of 4 or 8. They use vector stores (`_mm_storeu_si128`) which always write a full vector. If the image `width` is not a multiple of the vector size, the last store in each row will write past the allocated buffer.

### Impact
Heap corruption or corruption of adjacent data in the image buffer.

### Vulnerable Code (`backend/src/mandelbrot.c`)
```c
for (int px = 0; px < width; px += 8) {
  // ... (compute)
  _mm_storeu_si128((__m128i *)&buffer[py * width + px], v_iter_16); // Writes 16 bytes regardless of width
}
```

### Recommended Fix
Add a check to use a scalar fallback for the remaining pixels at the end of each row.

```c
for (int px = 0; px <= width - 8; px += 8) {
  // ... (SIMD compute and store)
}
// Scalar fallback for remainder
for (int px = (width / 8) * 8; px < width; px++) {
  // ... (Scalar compute and store)
}
```

---

## 5. Low: Path Traversal Vulnerability

### Description
The web server's path traversal protection relies solely on `strstr(path, "..")`. While this catches simple attacks, it is not as robust as resolving the absolute path.

### Vulnerable Code (`backend/src/webserver.c`)
```c
if (strstr(path, "..")) {
    // 403 Forbidden
}
```

### Recommended Fix
Use `realpath` to resolve the path and verify it still resides within the intended `root_path`.

---

## 6. General: Unchecked System Calls and Memory Allocations

### Description
The codebase consistently ignores the return values of `malloc`, `strdup`, `pthread_create`, and `recv`.

### Impact
- **Memory Exhaustion:** Null pointer dereferences.
- **Network Errors:** `receive_frame` may enter an infinite loop if `recv` returns `-1` (error) or `0` (closed), as it adds the return value to `total_read`.

### Recommended Fix
Always validate return values:
```c
uint8_t *buffer = malloc(payload_len + 1);
if (!buffer) return 0; // Proper check

ssize_t ret = recv(client_fd, buffer + total_read, payload_len - total_read, 0);
if (ret <= 0) { free(buffer); return 0; } // Handle error/close
total_read += ret;
```
