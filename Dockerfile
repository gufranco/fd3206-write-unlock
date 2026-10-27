# SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com>
# SPDX-License-Identifier: MIT

FROM debian:trixie@sha256:4c25275446ca371766b8b093c696782eabce06263fccacb8d5d813b3dc44b308

ARG CPPCHECK_VERSION=2.22.0
ARG CPPCHECK_SHA256=d74945deb2d50393430e07596b766f8a779512c7f60dac2a30ea64e059ece57b

ARG DEBIAN_SNAPSHOT=20261008T000000Z

RUN rm -f /etc/apt/sources.list.d/debian.sources \
 && printf '%s\n' \
      'Types: deb' \
      "URIs: http://snapshot.debian.org/archive/debian/${DEBIAN_SNAPSHOT}" \
      'Suites: trixie trixie-updates' \
      'Components: main' \
      'Check-Valid-Until: no' \
      'Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg' \
      '' \
      'Types: deb' \
      "URIs: http://snapshot.debian.org/archive/debian-security/${DEBIAN_SNAPSHOT}" \
      'Suites: trixie-security' \
      'Components: main' \
      'Check-Valid-Until: no' \
      'Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg' \
      > /etc/apt/sources.list.d/snapshot.sources

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      gcc-avr=1:14.2.0-2 \
      avr-libc=1:2.2.1-1 \
      binutils-avr \
      libsimavr-dev=1.6+dfsg-3+b3 \
      libelf-dev \
      pkg-config \
      gcc \
      libc6-dev \
      make \
      cmake \
      g++ \
      clang-format=1:19.0-63 \
      gcovr \
      python3 \
      python3-venv \
      curl \
      ca-certificates \
      git \
      libmagic1 \
 && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL -o /tmp/cppcheck.tar.gz "https://github.com/danmar/cppcheck/archive/refs/tags/${CPPCHECK_VERSION}.tar.gz" \
 && echo "${CPPCHECK_SHA256}  /tmp/cppcheck.tar.gz" | sha256sum -c - \
 && tar -xzf /tmp/cppcheck.tar.gz -C /tmp \
 && cmake -S "/tmp/cppcheck-${CPPCHECK_VERSION}" -B /tmp/cppcheck-build -DCMAKE_BUILD_TYPE=Release -DUSE_MATCHCOMPILER=ON -DHAVE_RULES=OFF -DUSE_BOOST=Off \
 && cmake --build /tmp/cppcheck-build --parallel 2 \
 && cmake --install /tmp/cppcheck-build \
 && rm -rf /tmp/cppcheck.tar.gz "/tmp/cppcheck-${CPPCHECK_VERSION}" /tmp/cppcheck-build

COPY docker/requirements-tools.txt /tmp/requirements-tools.txt
RUN python3 -m venv /opt/tools \
 && /opt/tools/bin/pip install --no-cache-dir --require-hashes -r /tmp/requirements-tools.txt \
 && rm -f /tmp/requirements-tools.txt

ENV PATH=/opt/tools/bin:$PATH \
    FDSWU_TOOLCHAIN=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    RUFF_CACHE_DIR=/tmp/ruff-cache
WORKDIR /src
