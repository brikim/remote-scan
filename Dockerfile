# --- Stage 1: Build Environment ---
FROM debian:trixie-slim AS build
LABEL maintainer="BK"

RUN apt-get update && apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ninja-build \
    ca-certificates \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

ENV CC=gcc
ENV CXX=g++

WORKDIR /app

COPY . .

RUN cmake -G Ninja -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON && \
    cmake --build build --config Release --parallel 4

# --- Stage 2: Runtime Environment ---
FROM debian:trixie-slim AS runtime

RUN apt-get update && apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
    ca-certificates \
    tzdata \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

ENV TZ=America/Chicago
ENV CONFIG_PATH='/config'
ENV LOG_PATH='/logs'

COPY --from=build /app/remotescan /usr/local/bin/remotescan

RUN chmod +x /usr/local/bin/remotescan

CMD ["/usr/local/bin/remotescan"]
