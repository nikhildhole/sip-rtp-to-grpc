# Stage 1: build the gateway (full image)
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ca-certificates \
    pkg-config \
    git \
    libyaml-cpp-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libgrpc++-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy project and build
COPY . /src
RUN cmake -S . -B build && cmake --build build -- -j$(nproc)

# Stage 2: minimal runtime (distroless)
FROM gcr.io/distroless/cc-debian11:nonroot

WORKDIR /app

# Copy runtime artifacts only
COPY --from=builder /src/build/sip_rtp_gateway /app/sip_rtp_gateway
COPY --from=builder /src/config /app/config

# Expose SIP and RTP ports (metadata only)
EXPOSE 5060/udp
EXPOSE 20000-20100/udp

# Use the distroless nonroot user
USER nonroot

# Run the gateway with the bundled config
CMD ["/app/sip_rtp_gateway", "--config", "/app/config/gateway.yaml"]

