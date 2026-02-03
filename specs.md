# Project Specifications

## 1. Overview

This project is a high-performance, zero-dependency Mandelbrot set viewer. It consists of a C backend that performs heavy computational rendering and a vanilla JavaScript frontend for visualization and interaction. The system uses raw WebSockets for low-latency communication and supports multiple hardware acceleration modes including AVX-512 SIMD and GPU Compute Shaders.

## 2. Technical Stack & Constraints

### Backend

- **Language:** C
- **Concurrency:** POSIX Threads (pthreads)
- **SIMD:** Intel Intrinsics (AVX, AVX2, AVX-512)
- **GPU:** OpenGL 4.3+ Compute Shaders (via EGL for headless context)
- **Networking:** Raw TCP Sockets, Custom WebSocket Handshake (RFC 6455 subset), Custom HTTP/1.1 implementation.
- **Dependencies:** None (libc, libm, libpthread, libGL, libEGL).
- **Security:**
  - Path traversal protection in web server.
  - Buffer overflow protections.
  - Resource limits (queue sizes, thread counts).

### Frontend

- **Language:** Vanilla JavaScript (ES6+)
- **Rendering:** HTML5 Canvas API (2D Context)
- **Styling:** CSS3
- **Dependencies:** None.

### Build & Environment

- **Containerization:** Docker (Variants for Scalar/AVX and INTEL/NVIDIA GPU).
- **Build System:** GNU Make.

## 3. Features

### Core Rendering

- **Scalar Mode:** Standard double-precision floating-point arithmetic.
- **SIMD Mode:** Hardware-accelerated rendering using AVX, AVX2, or AVX-512 instructions (4-8 pixels processed in parallel per core).
- **GPU Mode:** Massively parallel rendering using OpenGL Compute Shaders.
  - **Optimizations:** Cardioid / Period-2 bulb early exit checking.
  - **Precision Switching:** Automatic fallback to `float` (FP32) for shallow zooms to improve performance, switching to `double` (FP64) for deep zooms.

### Web Server

- **Static File Serving:** Serves HTML, CSS, JS, and image assets from a configurable root directory (default `/var/www`).
- **Security:** Restricts file access to the root directory, preventing `../` traversal attacks.
- **Concurrency:** Dedicated thread for handling HTTP requests.

### Frontend Client

- **Interactive Viewport:** Drag-to-pan and Scroll-to-zoom.
- **Dynamic Resolution:** Adjustable resolution divider for performance tuning on slower clients.
- **Color Themes:** Multiple procedural color palettes (Ocean, Fire, Matrix, Rainbow, Zebra).
- **Animation System:** Keyframe-based animation with smooth interpolation (linear for coordinates, logarithmic for zoom).
- **Export:** Save current view as PNG.

## 4. Communication Protocol

### WebSocket Protocol

The system uses a custom text/binary protocol over WebSockets.

**1. Client Request (Text Frame)**
The client sends a comma-separated string containing viewport parameters:

```text
x_min, y_min, x_scale, y_scale, width, height, iterations
```

- `x_min`, `y_min`: Top-left coordinate in the complex plane (double).
- `x_scale`, `y_scale`: Step size per pixel in the complex plane (double).
- `width`, `height`: Dimensions of the requested image (integer).
- `iterations`: Maximum iteration count (integer).

**2. Server Response (Binary Frame)**
The server responds with a raw binary payload.

- **Format:** `uint16_t` array (Little Endian).
- **Content:** Iteration counts for each pixel (row-major order).
- **Size:** `width * height * 2` bytes.

### HTTP Endpoints

- `GET /`: Serves `index.html`.
- `GET /style.css`: Serves stylesheet.
- `GET /script.js`: Serves frontend logic.
- `GET /favicon.ico`: Serves favicon.
- **Error Handling:** Returns 400, 403, 404, 405, and 503 status codes appropriately.

## 5. Security Specifications

- **Input Validation:**
  - HTTP requests are parsed strictly; malformed requests are rejected.
  - WebSocket payloads are parsed using `sscanf` with strict format checking.
- **Resource Management:**
  - **HTTP Queue:** Limited to 100 pending requests; excess connections receive 503 Service Unavailable.
  - **Thread Pool:** Fixed number of worker threads (default: number of CPU cores) to prevent resource exhaustion.
- **Memory Safety:**
  - Use of `aligned_alloc` for SIMD compatibility.
  - Cyclic buffer reuse and dynamic resizing with bounds checking.
  - Path traversal checks (`strstr(path, "..")`) in file server.
