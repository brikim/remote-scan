# 1. Define the version
ARG UBUNTU_RELEASE=24.04

# --- Stage 1: Build & Chisel ---
FROM ubuntu:${UBUNTU_RELEASE} AS build
LABEL maintainer="BK"

# Re-declare the ARG so it's usable inside this stage
ARG UBUNTU_RELEASE

# Install build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ninja-build ca-certificates libssl-dev curl tar \
    && rm -rf /var/lib/apt/lists/*

# Install 'chisel' tool
RUN curl -sSL https://github.com/canonical/chisel/releases/download/v1.0.0/chisel_v1.0.0_linux_amd64.tar.gz | tar -xz -C /usr/local/bin

WORKDIR /app
COPY . .

# Build Remote-Scan
RUN cmake -G Ninja -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON && \
    cmake --build build --config Release --parallel 4

# Create the tiny rootfs using the SAME version variable
RUN mkdir -p /rootfs && \
    chisel cut --release ubuntu-${UBUNTU_RELEASE} --root /rootfs \
    base-files_base \
    base-files_release-info \
    ca-certificates_data \
    libc6_libs \
    libgcc-s1_libs \
    libstdc++6_libs \
    libssl3t64_libs \
    openssl_config \
    tzdata_zoneinfo

# Copy binary to rootfs
RUN mkdir -p /rootfs/usr/local/bin && \
    cp /app/build/remote-scan /rootfs/usr/local/bin/remote-scan && \
    chmod +x /rootfs/usr/local/bin/remote-scan

# --- Stage 2: Runtime Environment ---
FROM scratch AS runtime

ENV TZ=America/Chicago
ENV CONFIG_PATH='/config'
ENV LOG_PATH='/logs'

# Copy the entire sculpted filesystem
COPY --from=build /rootfs /

ENTRYPOINT ["/usr/local/bin/remote-scan"]