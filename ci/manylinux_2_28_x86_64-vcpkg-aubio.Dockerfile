#This Dockerfile sets up a manylinux_2_28_x86_64 environment with vcpkg
# and installs aubio dependencies using a custom triplet.
# It is based on the official manylinux image and includes necessary
# tools and libraries for building Python wheels with aubio support.
# Use the latest stable manylinux_2_28_x86_64 image

FROM quay.io/pypa/manylinux_2_28_x86_64:2025.11.11-1


RUN dnf -y install curl zip unzip tar ninja-build git make nasm
# Install vcpkg and checkout a specific commit for reproducibility
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg && \
    git -C /opt/vcpkg checkout da096fdc67db437bee863ae73c4c12e289f82789

ENV VCPKG_INSTALLATION_ROOT="/opt/vcpkg"
ENV PATH="${PATH}:/opt/vcpkg"

ENV VCPKG_DEFAULT_TRIPLET="x64-linux-pic"

# mkdir & touch -> workaround for https://github.com/microsoft/vcpkg/issues/27786
RUN bootstrap-vcpkg.sh && \
    mkdir -p /root/.vcpkg/ $HOME/.vcpkg && \
    touch /root/.vcpkg/vcpkg.path.txt $HOME/.vcpkg/vcpkg.path.txt && \
    vcpkg integrate install && \
    vcpkg integrate bash

COPY vcpkg-triplets/x64-linux-pic.cmake opt/vcpkg/custom-triplets/x64-linux-pic.cmake
COPY vcpkg.json opt/vcpkg/

ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/opt/vcpkg/installed/x64-linux-pic/lib"
RUN vcpkg install --overlay-triplets=opt/vcpkg/custom-triplets \
    --feature-flags="versions,manifests" \
    --x-manifest-root=opt/vcpkg \
    --x-install-root=opt/vcpkg/installed \
    --clean-after-build && \
    vcpkg list && \
    rm -rf /opt/vcpkg/buildtrees /opt/vcpkg/downloads
ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/opt/vcpkg/installed/x64-linux-pic/lib:/opt/vcpkg/installed/x64-linux/lib"
# setting git safe directory is required for properly building wheels when
# git >= 2.35.3
RUN git config --global --add safe.directory "*"
