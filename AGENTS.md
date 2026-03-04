# AGENTS.md - Coding Guidelines for AI Agents

## Project Overview

A zero-dependency Mandelbrot set renderer written in C with a vanilla JavaScript frontend. Features hardware acceleration via AVX/AVX-512 SIMD and OpenGL compute shaders.

## Build Commands

### Backend (C)
```bash
# Build server binary
cd backend && make

# Build with debug logging
cd backend && make debug

# Build and run benchmark
cd backend && make bench

# Build for memory debugging (valgrind)
cd backend && make valgrind

# Clean build artifacts
cd backend && make clean
```

### Docker Deployment
```bash
# Scalar mode (universal)
docker-compose -f docker-compose.scalar.yml up --build

# AVX SIMD mode (requires AVX support)
docker-compose -f docker-compose.simd.yml up --build

# Intel GPU mode
docker-compose -f docker-compose.intel.yml up --build

# NVIDIA GPU mode (requires NVIDIA Container Toolkit)
docker-compose -f docker-compose.nvidia.yml up --build
```

### Testing
```bash
# Manual WebSocket test (requires running server)
cd backend && python3 test_one_frame.py

# Run benchmark suite
./backend/run_bench.sh
```

## Code Style Guidelines

### C Code

- **Indentation**: 4 spaces (enforced by .clang-format)
- **Naming**:
  - Functions/variables: `snake_case`
  - Macros/constants: `UPPER_CASE`
  - Types: `snake_case_t` suffix for typedefs
  - Structs: avoid typedef where possible, use `struct name`
- **Header guards**: Use `#ifndef _FILENAME_H_` / `#define _FILENAME_H_`
- **Includes**: Order - local headers (quotes) before system headers (angle brackets)

### Example Structure
```c
#include "include/module.h"
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 4096

void function_name(int param) {
    // implementation
}
```

### JavaScript

- **Frontend**: Vanilla JS in `backend/frontend/`
- **Style**: ES6+ classes, camelCase for methods/variables
- **WebSocket**: Custom binary protocol for Mandelbrot rendering

### Comments

- Document complex mathematical algorithms (see mandelbrot.c for examples)
- Use inline comments for non-obvious optimizations
- Comment thread synchronization logic and race conditions

### Error Handling

- Always check return values from pthread functions
- Use `fprintf(stderr, ...)` for errors
- Implement graceful degradation for GPU initialization failures
- Free allocated memory in error paths

## Architecture

### Key Components

- **server.c**: Main entry, socket handling, client threads
- **mandelbrot.c**: Mathematical core (scalar, AVX, OpenGL implementations)
- **tpool.c**: Thread pool for CPU rendering tasks
- **websocket.c**: RFC-6455 WebSocket protocol implementation
- **webserver.c**: HTTP server for static files

### Threading Model

- Main thread accepts connections
- One thread per WebSocket client
- Thread pool for CPU rendering (default: CPU core count)
- Single dedicated GPU thread with OpenGL context

### Memory Safety

- Use `valgrind` target for memory leak detection
- Free job structures after execution in worker threads
- Proper mutex/condition variable cleanup on shutdown

## Security

- Validate file paths in webserver (prevent `..` traversal)
- Check WebSocket frame bounds
- No external dependencies reduces attack surface

## Environment Variables

- `MANDELBROT_THREADS`: Number of worker threads (default: CPU count)
- `MANDELBROT_SIMD`: Enable/disable SIMD (0=scalar, 1=AVX)

## Debugging

- Use `make debug` to enable DEBUG() macro output
- Build with `make valgrind` for memory debugging
- Use `test_one_frame.py` for isolated WebSocket testing
