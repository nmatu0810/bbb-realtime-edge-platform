FROM ubuntu:24.04 

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8
# (2) Gói build host cho Yocto (Ubuntu/Debian)
RUN apt-get update && apt-get install -y --no-install-recommends \
      gawk wget git diffstat unzip texinfo gcc build-essential chrpath \
      socat cpio python3 python3-pip python3-pexpect python3-git \
      python3-jinja2 python3-subunit xz-utils zstd liblz4-tool file \
      debianutils iputils-ping libacl1 locales ca-certificates \
    && locale-gen en_US.UTF-8 \
    && rm -rf /var/lib/apt/lists/*

# (3) User trùng uid host, để file ghi vào thư mục mount không thuộc root
ARG UID=1000
RUN userdel -r ubuntu 2>/dev/null || true; \
    useradd -m -u ${UID} -s /bin/bash builder

USER builder
WORKDIR /workspace
