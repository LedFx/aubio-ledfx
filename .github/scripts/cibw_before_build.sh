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

# Set SUDO command, if not running as root
SUDO=""
if [ "$(id -u)" != "0" ]; then
  SUDO="sudo"
fi

if command -v apt-get >/dev/null 2>&1; then
  echo "[cibw_before_build] apt-get found, installing packages"
  $SUDO apt-get update
  $SUDO apt-get install -y --no-install-recommends \
    pkg-config libsndfile1-dev libsamplerate0-dev libfftw3-dev libavcodec-dev \
    libavformat-dev libavutil-dev libswresample-dev libvorbis-dev libflac-dev \
    libjack-dev librubberband-dev ffmpeg
elif command -v yum >/dev/null 2>&1; then
  if [ "$(uname -m)" = "aarch64" ]; then
    echo "[cibw_before_build] aarch64 yum environment detected, installing epel and building ffmpeg from source"
    $SUDO yum -y install epel-release
    $SUDO yum -y groupinstall "Development Tools"
    $SUDO yum -y install nasm cmake pkgconfig libsndfile-devel libsamplerate-devel fftw-devel \
      libvorbis-devel flac-devel rubberband-devel
    curl -fsSL https://ffmpeg.org/releases/ffmpeg-8.0.tar.xz -o ffmpeg.tar.xz
    tar xJf ffmpeg.tar.xz
    cd ffmpeg-8.0
    ./configure --prefix=$DESTDIR/usr/local --arch=aarch64 --enable-shared
    make -j4
    make install
    cd ..
  else
    echo "[cibw_before_build] yum found, installing packages"
    $SUDO yum -y install epel-release
    if ! rpm -q rpmfusion-free-release >/dev/null 2>&1; then
      $SUDO yum -y install --nogpgcheck https://download1.rpmfusion.org/free/el/rpmfusion-free-release-7.noarch.rpm
    fi
    $SUDO yum -y install pkgconfig libsndfile-devel libsamplerate-devel fftw-devel \
      ffmpeg-devel libvorbis-devel flac-devel rubberband-devel
  fi
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
WAF_CONFIGURE_OPTS="--prefix=$DESTDIR/usr/local --enable-fftw3 --enable-avcodec --enable-sndfile --enable-rubberband --jobs=2"
echo "[cibw_before_build] WAF_CONFIGURE_OPTS=$WAF_CONFIGURE_OPTS"

# The Python executable in the cibuildwheel environment is the one for the current build.
echo "[cibw_before_build] install numpy"
python -m pip install numpy==1.26.4

echo "[cibw_before_build] running: python waf configure"
case "$(uname -s)" in
  MSYS*|MINGW*)
    echo "[cibw_before_build] Windows detected, using gcc"
    CC=gcc CXX=g++ python waf configure $WAF_CONFIGURE_OPTS
    ;;
  *)
    python waf configure $WAF_CONFIGURE_OPTS
    ;;
esac

echo "[cibw_before_build] running: python waf build"
python waf build

echo "[cibw_before_build] running: python waf install"
python waf install

echo "[cibw_before_build] installed aubio to $DESTDIR"
echo "[cibw_before_build] done"
exit 0