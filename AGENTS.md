# AGENTS.md: Multithreaded Mandelbrot Viewer

This document outlines the specialized agent roles and the implementation roadmap for the zero-dependency, SIMD-accelerated Mandelbrot project.

---

## 1. Technical Stack Constraints
* **Backend:** C11, POSIX Threads, Intel Intrinsics (AVX2), OpenGL, no other libraries.
* **Frontend:** Vanilla JS (ES6), CSS3, HTML5 Canvas, no other libraries.
* **Network:** Raw WebSockets (no `libwebsockets` or similar).
* **Build System:** Makefile + Docker (for consistent environment).
