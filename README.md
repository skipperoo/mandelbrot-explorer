# MANDELBROT EXPLORER

(c) 2026 - Leonardo Scoppitto

[ WHAT IS THIS? ]

---

A zero-dependency, hardware-accelerated Mandelbrot set renderer.
No frameworks. No bloat. Just raw C11 and optimized Assembly.

- BACKEND....: C11, POSIX Threads, AVX-512, OpenGL Compute Shaders
- FRONTEND...: Vanilla JS, HTML5 Canvas
- NETWORKING.: Raw TCP Sockets, Custom RFC-6455 WebSocket Implementation

[ SYSTEM REQUIREMENTS ]

---

- Linux Environment
- Docker & Docker Compose
- Optional: Intel CPU (AVX2/512) or NVIDIA GPU for hardware acceleration

[ COMPILATION & EXECUTION ]

---

Select your hardware profile and run:

1. SCALAR MODE (Compatible with everything)

```bash
docker-compose -f docker-compose.scalar.yml up --build
```

1. INTEL SIMD MODE (Requires AVX support)

```bash
docker-compose -f docker-compose.intel.yml up --build
```

1. INTEL GPU MODE (Check the device passthrough)

```bash
docker-compose -f docker-compose.intel.yml up --build
```

1. NVIDIA GPU MODE (Requires Nvidia Container Toolkit)

```bash
docker-compose -f docker-compose.nvidia.yml up --build
```

[ ACCESS ]

---

Point your browser to: <http://localhost:8080>

[ CONTROLS ]

---

- LEFT MOUSE ... Pan view
- SCROLL ....... Zoom In/Out
- DOUBLE CLICK . Reset View
- UI PANELS .... Change Resolution, Theme, or Record Animations

[ ARCHITECTURE NOTES ]

---

See [specs.md] and [docs.md] for more information.

- SERVER runs on port 8080.
- WEBSERVER serves static files from `/var/www`.
- COMPUTE ENGINE supports dynamic precision switching (FP32/FP64).

[ EOF ]
