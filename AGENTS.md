# AGENTS.md: Multithreaded Mandelbrot Viewer

This document outlines the specialized agent roles and the implementation roadmap for the zero-dependency, SIMD-accelerated Mandelbrot project.

---

## 1. Agent Roles

| Agent | Focus Area | Responsibilities |
| :--- | :--- | :--- |
| **Architect** | Protocol & Interface | Defining the binary data format for frame transmission and the zoom-state sync logic. |
| **Backend Specialist** | C / Systems Programming | Implementing the raw TCP server, the `pthread` pool, and the WebSocket handshake logic. |
| **SIMD Expert** | Optimization | Writing the Mandelbrot kernel using `immintrin.h` for AVX2/AVX-512 vectorization. |
| **Frontend Lead** | UI & UX | Handling the `<canvas>` rendering, mouse-wheel zoom math, and animation path interpolation. |

---

## 2. Implementation Roadmap

### **Phase 1: The Socket Foundation (Zero-Dependency)**
* **Task 1.1:** Setup a raw C TCP server (AF_INET, SOCK_STREAM).
* **Task 1.2:** Implement the **WebSocket Handshake**. This requires a custom SHA-1 and Base64 encoder to generate the `Sec-WebSocket-Accept` header.
* **Task 1.3:** Implement **Frame Parsing**. Logic to handle unmasked/masked packets and OpCodes.

### **Phase 2: The High-Performance Kernel**
* **Task 2.1:** Basic scalar Mandelbrot in C.
* **Task 2.2:** **AVX2 Vectorization**. Process 4 `double` values ($z = z^2 + c$) in a single `__m256d` register.
* **Task 2.3:** **Multithreading**. Divide the screen into vertical "strips" or "tiles" and distribute them across a thread pool.

### **Phase 3: Frontend & Interaction**
* **Task 3.1:** Create the Canvas-based viewer in `index.html`.
* **Task 3.2:** Implement the **Zoom Logic**. Calculate the new complex plane boundaries based on mouse coordinates.
* **Task 3.3:** Binary Data Handling. Efficiently convert the incoming `ArrayBuffer` from the C backend into `ImageData` for the canvas.

### **Phase 4: Animation & Export**
* **Task 4.1:** **Path Interpolation**. Create a system to define "Waypoints" (x, y, zoom) and calculate smooth transitions.

### **Phase 5: Implement GPU acceleration**
* **Task 5.1:** **OpenGL rendering**. Using the signature `void render_opengl(uint16_t *buffer, int width, int start_row, int end_row, int max_iterations, double x_min, double y_min, double x_scale, double y_scale);` implement the gpu rendering inside the `mandelbrot.c` file.
* **Task 5.2:** **Configuration**. Allow the backend to be confiugured using an environment variable `MANDELBROT_GPU=1` to trigger GPU rendering and ignoring the already set `MANDELBROT_SCALAR` variable.
* **Task 5.3:** **Docker deployment**. Pass the GPU to the docker container using the docker-compose file.


---

## 3. Technical Stack Constraints
* **Backend:** C11, POSIX Threads, Intel Intrinsics (AVX2), OpenGL, no other libraries.
* **Frontend:** Vanilla JS (ES6), CSS3, HTML5 Canvas, no other libraries.
* **Network:** Raw WebSockets (no `libwebsockets` or similar).
* **Build System:** Makefile + Docker (for consistent environment).
