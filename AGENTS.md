# AGENTS.md: Multithreaded Mandelbrot Viewer

This document outlines the specialized agent roles and the implementation roadmap for the zero-dependency, SIMD-accelerated Mandelbrot project.

---

## 1. Technical Stack Constraints
* **Backend:** C11, POSIX Threads, Intel Intrinsics (AVX2), OpenGL, no other libraries.
* **Frontend:** Vanilla JS (ES6), CSS3, HTML5 Canvas, no other libraries.
* **Network:** Raw WebSockets (no `libwebsockets` or similar).
* **Build System:** Makefile + Docker (for consistent environment).

---

## 2. Next features

As of now the project is composed by a frontend and a backend. The frontend is served by an nignx instance and consists in 3 static files:
- index.html
- script.js
- style.css

Now we want to:
- [x] implement a webserver in our backend.
- [x] make the backend serve both the mandelbrot generation and the webpages, being aware to implement the basic security features such as avoid use after free, no buffer overflows and so on.
- [x] the amount of requests is small so the webserver should run in its own thread with its own request queue.
- [x] The page should be served from `/var/www` and the requests should not escape from that path.

A note on the backend project structure:
```
```
```
```text

.
├── benchmark.log <- ignore it
├── Dockerfile <- dockerfile without nvidia framework
├── Dockerfile.nvidia <- dockerfile with nvidia framework
├── frontend <- frontend directory
│  ├── index.html
│  ├── script.js
│  └── style.css
├── Makefile <- makefile for compilation
├── plot_benchmark.py <- ignore it
├── src <- C backend source
│  ├── include <- headers
│  ├── mandelbrot.c <- mandelbrot rendering
│  ├── server.c <- entrypoint and server implementation
│  ├── tpool.c <- mandelbrot threadpool implementation (for the webserver use another thread)
│  ├── webserver.c <- Implement webserver HERE
│  ├── websocket.c <- websocket handshake and send/recieve
│  └── websocket_utils.c <- websocket  utils functions
└── test_one_frame.py```
```
```
```
```
```
```
```
```
