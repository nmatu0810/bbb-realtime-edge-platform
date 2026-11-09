FROM ubuntu:24.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libreoffice-writer-nogui python3-uno fonts-liberation fonts-dejavu-core poppler-utils && rm -rf /var/lib/apt/lists/*
# ubuntu:24.04 đã có user ubuntu uid 1000
USER ubuntu
WORKDIR /w
