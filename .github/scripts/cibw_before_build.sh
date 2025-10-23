#!/bin/sh
set -euo pipefail
echo "[cibw_before_build] starting"

# Project root inside cibuildwheel build env is usually /project or current working dir
PROJECT_DIR="$PWD"
DESTDIR="$PROJECT_DIR/build/dist"
mkdir -p "$DESTDIR"

echo "[cibw_before_build] project: $PROJECT_DIR"
echo "[cibw_before_build] destdir: $DESTDIR"

# Try to install platform package deps where possible (best-effort)
if command -v apt-get >/dev/null 2>&1; then
  echo "[cibw_before_build] apt-get found, attempting to install packages (best-effort)"
  sudo apt-get update || true
  sudo apt-get install -y --no-install-recommends \
    pkg-config libsndfile1-dev libsamplerate0-dev libfftw3-dev libavcodec-dev libavformat-dev libavutil-dev libswresample-dev libvorbis-dev libflac-dev libjack-dev jackd2 librubberband-dev || true
elif command -v yum >/dev/null 2>&1; then
  echo "[cibw_before_build] yum found, attempting to install packages (best-effort)"
  sudo yum -y install epel-release || true
  sudo yum -y install pkgconfig libsndfile libsndfile-devel libsamplerate libsamplerate-devel fftw fftw-devel ffmpeg ffmpeg-devel libvorbis libvorbis-devel flac flac-devel || true
elif command -v pacman >/dev/null 2>&1; then
  echo "[cibw_before_build] pacman found, attempting to install packages (best-effort)"
  pacman -Sy --noconfirm pkgconf libsndfile libsamplerate fftw ffmpeg libvorbis flac rubberband || true
else
  echo "[cibw_before_build] no known package manager found; continuing without system package installation"
fi

# Fetch waf if not present
if [ ! -x ./waf ]; then
  echo "[cibw_before_build] downloading waf"
  curl -fsSL -o waf https://waf.io/waf-2.0.27
  chmod +x waf || true
fi

# Configure, build and install aubio into DESTDIR using waf
export WAFOPTS="--enable-fftw3 --enable-avcodec --enable-sndfile --enable-rubberband --destdir $DESTDIR --jobs 2"
echo "[cibw_before_build] WAFOPTS=$WAFOPTS"
echo "[cibw_before_build] install numpy"
python -m pip install numpy
echo "[cibw_before_build] running: python waf configure"
python waf configure $WAFOPTS
echo "[cibw_before_build] running: python waf build"
python waf build $WAFOPTS
echo "[cibw_before_build] running: python waf install"
python waf install $WAFOPTS

echo "[cibw_before_build] installed aubio to $DESTDIR"

echo "[cibw_before_build] done"
exit 0
