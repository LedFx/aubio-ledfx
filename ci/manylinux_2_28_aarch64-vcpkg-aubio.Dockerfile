#This Dockerfile sets up a manylinux_2_28_aarch64 environment with vcpkg
# and installs aubio dependencies using a custom triplet.
# It is based on the official manylinux image and includes necessary
# tools and libraries for building Python wheels with aubio support.
# Use the latest stable manylinux_2_28_aarch64 image

FROM quay.io/pypa/manylinux_2_28_aarch64:2025.11.11-1


RUN dnf -y install curl zip unzip tar ninja-build git make nasm

# Install vcpkg and checkout a specific commit for reproducibility
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg && \
    git -C /opt/vcpkg checkout da096fdc67db437bee863ae73c4c12e289f82789

ENV VCPKG_INSTALLATION_ROOT="/opt/vcpkg"
ENV PATH="${PATH}:/opt/vcpkg"

ENV VCPKG_DEFAULT_TRIPLET="arm64-linux-pic"
# pkgconf fails to build with default debug mode of arm64-linux host
ENV VCPKG_DEFAULT_HOST_TRIPLET="arm64-linux-release"

# Must be set when building on arm
ENV VCPKG_FORCE_SYSTEM_BINARIES=1

# mkdir & touch -> workaround for https://github.com/microsoft/vcpkg/issues/27786
RUN bootstrap-vcpkg.sh && \
    mkdir -p /root/.vcpkg/ $HOME/.vcpkg && \
    touch /root/.vcpkg/vcpkg.path.txt $HOME/.vcpkg/vcpkg.path.txt && \
    vcpkg integrate install && \
    vcpkg integrate bash

COPY vcpkg-triplets/arm64-linux-pic.cmake opt/vcpkg/custom-triplets/arm64-linux-pic.cmake
COPY vcpkg.json opt/vcpkg/

ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/opt/vcpkg/installed/arm64-linux-pic/lib"
RUN vcpkg install --overlay-triplets=opt/vcpkg/custom-triplets \
    --feature-flags="versions,manifests" \
    --x-manifest-root=opt/vcpkg \
    --x-install-root=opt/vcpkg/installed \
    --clean-after-build && \
    vcpkg list && \
    rm -rf /opt/vcpkg/buildtrees /opt/vcpkg/downloads
ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/opt/vcpkg/installed/arm64-linux-pic/lib:/opt/vcpkg/installed/arm64-linux-release/lib"
# setting git safe directory is required for properly building wheels when
# git >= 2.35.3
RUN git config --global --add safe.directory "*"