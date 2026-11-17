FROM ubuntu:22.04

MAINTAINER Robert Greiner <robert@robertgreiner.com>
LABEL DESCRIPTION="STUN Implementation for RFC 8489 - Session Traversal Utilities for NAT"

# Install build dependencies
RUN apt-get update && \
    apt-get install -y \
        build-essential \
        g++ \
        libssl-dev \
        cmake \
        python3-dev

# Create work directory
WORKDIR /app

# Copy project files
COPY . .

# Install STUN library
RUN mkdir -p build && cd build && \
    cmake .. && make -j$(nproc)

# Run tests
RUN ./test/stun_test || true

# Default command
CMD ["./stun"]
