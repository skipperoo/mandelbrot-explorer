# Mandelbrot Explorer

A zero-dependency, hardware-accelerated Mandelbrot set renderer. Built with raw C11, optimized Assembly, and Vanilla JS.

## Tech Stack
*   **Backend:** C11, POSIX Threads, AVX-512, OpenGL Compute Shaders
*   **Frontend:** Vanilla JS, HTML5 Canvas
*   **Networking:** Raw TCP Sockets, Custom RFC-6455 WebSocket Implementation

## Requirements
*   Linux Environment
*   Docker & Docker Compose
*   **Optional:** Intel CPU (AVX2/512) or NVIDIA GPU

## Usage

Select the appropriate hardware profile to build and run:

**Scalar Mode (Universal)**
```bash
docker-compose -f docker-compose.scalar.yml up --build
```

**Intel SIMD Mode (Requires AVX)**
```bash
docker-compose -f docker-compose.intel.yml up --build
```

**NVIDIA GPU Mode (Requires NVIDIA Container Toolkit)**
```bash
docker-compose -f docker-compose.nvidia.yml up --build
```

Access the application at **[http://localhost:8080](http://localhost:8080)**.

## Controls

| Action | Input |
|--------|-------|
| **Pan** | Left Mouse Drag |
| **Zoom** | Scroll Wheel |
| **Reset** | Double Click |
| **Settings** | UI Panels (Resolution, Theme, Animation) |

## Documentation

*   [specs.md](specs.md) - Technical specifications and protocol definitions.
*   [docs.md](docs.md) - Architecture overview and implementation details.

---
(c) 2026 - Leonardo Scoppitto
