# --- Stage 1: Build Environment ---
FROM debian:trixie-slim AS build
LABEL maintainer="Your Name"

# Install build dependencies: g++, cmake, make, etc.
RUN echo "deb http://deb.debian.org/debian sid main" | tee -a /etc/apt/sources.list && \
	apt update && \
	apt upgrade -y && \
    apt install -y --no-install-recommends \
    build-essential \
	gcc-14 g++-14 \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source code into the container
COPY . /app

# Configure and build the project
RUN cmake -DCMAKE_BUILD_TYPE=Release . && \
    cmake --build . -j 4

# --- Stage 2: Runtime Environment ---
FROM debian:trixie-slim AS runtime

ENV TZ=America/Chicago
ENV CONFIG_PATH='/config'

RUN apt update && \
	apt upgrade -y && \
	rm -rf /var/lib/apt/lists/*

# Copy only the compiled executable from the 'build' stage
COPY --from=build /app/remotescan /usr/local/bin/remotescan

# Command to run the application
CMD ["/usr/local/bin/remotescan"]
