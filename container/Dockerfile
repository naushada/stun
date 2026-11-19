FROM ubuntu:22.04

MAINTAINER Robert Greiner <robert@robertgreiner.com>
LABEL DESCRIPTION="STUN Implementation for RFC 8489 - Session Traversal Utilities for NAT"

# Fix broken apt lists and update packages
RUN apt-get update --fix-broken && \
    apt-get install -y build-essential libssl-dev cmake && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

# Build STUN library using Linux GCC (use g++-11 to avoid symlinks issues)
RUN bash -c 'cd /app/build && g++ --version | head -1 && cmake .. >/dev/null 2>&1 && make -j4'

# Copy built binary to bin directory  
RUN cp /app/build/test_stun /app/bin/

# Default command  
CMD ["./bin/test_stun"]
