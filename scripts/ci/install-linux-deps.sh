#!/usr/bin/env bash
# Build dependencies for SDL3 on Ubuntu (per https://wiki.libsdl.org/SDL3/README-linux) plus
# Ninja and Clang. SDL 3.4 stops configuring when an enabled backend lacks its -dev package.
# Usage: scripts/ci/install-linux-deps.sh [extra packages...]
set -euo pipefail
sudo apt-get update -qq
sudo apt-get install -y -qq --no-install-recommends \
  ninja-build clang pkg-config \
  libasound2-dev libpulse-dev libpipewire-0.3-dev libjack-dev libsndio-dev libaudio-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
  libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
  libdrm-dev libgbm-dev libegl-dev libgl-dev libgles-dev \
  libdbus-1-dev libibus-1.0-dev libudev-dev libthai-dev liburing-dev libfribidi-dev \
  "$@"
