#!/bin/sh
set -euo pipefail

echo "[cibw_before_build] starting"

# The project root in the cibuildwheel environment is usually /project,
# but can be the current working directory.
PROJECT_DIR="$PWD"
DESTDIR="$PROJECT_DIR/build/dist"
mkdir -p "$DESTDIR"

echo "[cibw_before_build] project: $PROJECT_DIR"
echo "[cibw_before_build] destdir: $DESTDIR"

# Install platform-specific package dependencies where possible (best-effort).
# These are dependencies for building libaubio, not for the Python wheel itself.

# Set SUDO command, if available
SUDO=""
if command -v sudo >/dev/null 2>&1; then
  SUDO="sudo"
fi

if command -v apt-get >/dev/null 2>&1; then
  echo "[cibw_before_build] apt-get found, installing packages"
  $SUDO apt-get update
  $SUDO apt-get install -y --no-install-recommends \
    pkg-config libsndfile1-dev libsamplerate0-dev libfftw3-dev libavcodec-dev \
    libavformat-dev libavutil-dev libswresample-dev libvorbis-dev libflac-dev \
    libjack-dev librubberband-dev
elif command -v yum >/dev/null 2>&1; then
  echo "[cibw_before_build] yum found, installing packages"
  $SUDO yum -y install epel-release
  $SUDO yum -y install pkgconfig libsndfile-devel libsamplerate-devel fftw-devel \
    ffmpeg-devel libvorbis-devel flac-devel
elif command -v pacman >/dev/null 2>&1; then
  echo "[cibw_before_build] pacman found, installing packages"
  $SUDO pacman -Sy --noconfirm pkgconf libsndfile libsamplerate fftw ffmpeg libvorbis flac rubberband
else
  echo "[cibw_before_build] no known package manager found, continuing without system package installation"
fi

# Fetch waf if it's not already in the project directory.
if [ ! -x ./waf ]; then
  echo "[cibw_before_build] downloading waf"
  curl -fsSL -o waf https://waf.io/waf-2.0.27
  chmod +x waf
fi

# Configure, build, and install libaubio into DESTDIR using waf.
# These options are for building the C library, not the Python extension.
WAFOPTS="--enable-fftw3 --enable-avcodec --enable-sndfile --enable-rubberband --destdir=$DESTDIR --jobs=2"
echo "[cibw_before_build] WAFOPTS=$WAFOPTS"

# The Python executable in the cibuildwheel environment is the one for the current build.
echo "[cibw_before_build] install numpy"
python -m pip install numpy

echo "[cibw_before_build] running: python waf configure"
python waf configure $WAFOPTS

echo "[cibw_before_build] running: python waf build"
python waf build

echo "[cibw_before_build] running: python waf install"
python waf install

echo "[cibw_before_build] installed aubio to $DESTDIR"
echo "[cibw_before_build] done"
exit 0