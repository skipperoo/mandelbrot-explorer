# Security Patch Analysis Report: Mandelbrot Web Server

This report documents the verification of security fixes applied to address vulnerabilities identified in the original `security_report.md`.

---

## 1. Websocket Frame Handling: Stack-based Buffer Overflow
**Status: PARTIALLY FIXED**

### Findings
The function signature of `receive_frame` in `backend/src/websocket.c` has been updated to include `size_t max_len`. However, the implementation **does not use this parameter** to perform bounds checking before copying data into `out_payload`.
An attacker can still provide a `payload_len` larger than `max_len`, causing a buffer overflow on the stack when the unmasked data is written.

### Vulnerable Code (`backend/src/websocket.c`)
```c
int receive_frame(int client_fd, char *out_payload, size_t max_len) {
  // ...
  // payload_len is parsed from header but NOT checked against max_len
  // ...
  if (is_masked) {
    for (size_t i = 0; i < payload_len; i++) {
      out_payload[i] = buffer[i] ^ mask_key[i % 4]; // Potential Overflow
    }
  }
}
```

---

## 2. Websocket Handshake: Buffer Overflow
**Status: FIXED**

### Findings
The `perform_handshake` function in `backend/src/websocket.c` now correctly validates the length of the `Sec-WebSocket-Key` before copying it into the `key` buffer using `KEY_LEN` (defined as 64 in `websocket.h`).

### Verified Code
```c
  char key[KEY_LEN];
  int key_len = key_end - key_start;
  if (key_len >= KEY_LEN)
    return -1;
```

---

## 3. Thread Pool Task Overwrite and Memory Leak
**Status: FIXED**

### Findings
`tpool_add_work` in `backend/src/tpool.c` now includes a check to see if the queue is full. If the queue is full, it drops the task and jumps to the unlock label, preventing overwrites of pending tasks.

### Verified Code
```c
  if (pool->count + 1 > pool->queue_size)
    goto unlock_tp;
```

---

## 4. SIMD Memory Corruption (Out-of-bounds Write)
**Status: FIXED**

### Findings
The `render_simd` function in `backend/src/mandelbrot.c` now correctly calculates `vec_width` to be a multiple of the vector size and uses a scalar fallback for the remaining pixels in each row. It also uses `_mm_storel_epi64` for 64-bit (4 pixels) writes, which matches the 2-byte-per-pixel `uint16_t` format.

### Verified Code
```c
  int vec_width = width & ~3; 
  // ... vectorized loop ...
  for (int px = vec_width; px < width; px++) {
      // ... scalar fallback ...
  }
```

---

## 5. Path Traversal Vulnerability
**Status: FIXED**

### Findings
The `serve_file` function in `backend/src/webserver.c` has been completely rewritten to use `realpath()` for path resolution and `strncmp()` to ensure the resolved path resides within the `root_path`. This is a robust defense against path traversal attacks.

### Verified Code
```c
  if (realpath(raw_path, resolved_path) == NULL) { ... }
  if (strncmp(resolved_path, root_path, strlen(root_path)) != 0) { ... }
```

---

## 6. General: Unchecked System Calls and Memory Allocations
**Status: PARTIALLY FIXED**

### Findings
- **Allocations:** Most allocations now use the `CHECKALLOC` macro which exits the program on failure.
- **Pthreads:** Pthread calls are wrapped in `CHECKPTHREAD`.
- **Network I/O:** `receive_frame` in `backend/src/websocket.c` still lacks robust error handling for `recv`. Specifically:
  - If `recv` fails in the data reading loop, `total_read` will be corrupted.
  - If `recv` returns 0 (connection closed), the data loop may become infinite.

### Vulnerable Code (`backend/src/websocket.c`)
```c
  while (total_read < payload_len) {
    total_read +=
        recv(client_fd, buffer + total_read, payload_len - total_read, 0);
  }
```

---

## Summary
| Vulnerability | Status | Notes |
| :--- | :--- | :--- |
| Websocket Buffer Overflow | **PARTIALLY FIXED** | `max_len` parameter ignored. |
| Handshake Buffer Overflow | **FIXED** | Length check implemented. |
| Thread Pool Overwrite | **FIXED** | Task dropping implemented. |
| SIMD Out-of-bounds Write | **FIXED** | Scalar fallback implemented. |
| Path Traversal | **FIXED** | `realpath` verification implemented. |
| General Error Handling | **PARTIALLY FIXED** | `recv` still lacks error checks in some places. |
