# backend/Dockerfile

# --- Build Stage ---
FROM gcc:12 AS builder

WORKDIR /app

# Install dependencies (if any specific headers are needed, usually gcc has stdlibs)
# Copy source code
COPY . .

# Build the project
# We ensure architecture flags are set for AVX2. 
# NOTE: The host machine MUST support AVX2 for this to run.
RUN make clean && make -j

# --- Runtime Stage ---
FROM debian:bookworm-slim

WORKDIR /app

# Copy the binary from the builder stage
# Adjust the path "bin/server" if your Makefile outputs somewhere else
COPY --from=builder /app/bin/server /app/server

# Ensure execution permissions
RUN chmod +x /app/server

# Expose the backend port
EXPOSE 8080

# Run the server with Thread Count arguments (optional)
# We use shell form to allow passing args via docker-compose if needed
CMD ["./server"]
