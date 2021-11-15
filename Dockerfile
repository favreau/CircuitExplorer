# Note: Cyrille's branch at https://github.com/favreau/Brayns is used instead of https://github.com/BlueBrain/Brayns.git
# Docker container for running the CircuitExplorer as a plugin within Brayns as a service
# Check https://docs.docker.com/engine/userguide/eng-image/dockerfile_best-practices/#user for best practices.
# Based on the Dockerfile of Brayns created by the viz team (basically a copy)

# This Dockerfile leverages multi-stage builds, available since Docker 17.05
# See: https://docs.docker.com/engine/userguide/eng-image/multistage-build/#use-multi-stage-builds

# Image where Brayns+CircuitExplorer plugin is built
# FROM debian:buster-slim as builder
FROM ubuntu:20.04 as builder
LABEL maintainer="cyrille.favreau@epfl.ch"
ARG DIST_PATH=/app/dist
WORKDIR /app

ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moscow

# CMake 3.15.5 (required by Brion)
RUN apt-get update
RUN apt-get install -y curl unzip tar wget git
RUN wget -O cmake.sh https://github.com/Kitware/CMake/releases/download/v3.16.3/cmake-3.16.3-Linux-x86_64.sh && sh ./cmake.sh --prefix=/usr/local --skip-license

# Install packages
RUN apt-get -y --no-install-recommends install \
    build-essential \
    git \
    ninja-build \
    libarchive-dev \
    libassimp-dev \
    libboost-date-time-dev \
    libboost-filesystem-dev \
    libboost-iostreams-dev \
    libboost-program-options-dev \
    libboost-regex-dev \
    libboost-serialization-dev \
    libboost-system-dev \
    libboost-test-dev \
    libfreeimage-dev \
    libhdf5-serial-dev \
    libtbb-dev \
    libturbojpeg0-dev \
    libuv1-dev \
    libpqxx-dev \
    pkg-config \
    ca-certificates \
    libcgal-dev \
    libtiff-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# --------------------------------------------------------------------------------
# Install Assimp
# https://github.com/assimp/assimp
# --------------------------------------------------------------------------------
ARG ASSIMP_TAG=v4.1.0
ARG ASSIMP_SRC=/app/assimp

RUN rm -rf ${ASSIMP_SRC} \
    && mkdir -p ${ASSIMP_SRC} \
    && cd ${ASSIMP_SRC} \
    && git clone https://github.com/assimp/assimp ${ASSIMP_SRC}

RUN cd ${ASSIMP_SRC} \
    && git -c advice.detachedHead=false checkout ${ASSIMP_TAG} \
    && git submodule update --init --recursive \
    && mkdir -p build \
    && cd build \
    && cmake .. -Wno-dev \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} || exit 0

RUN cd ${ASSIMP_SRC}/build && make -j install
# RUN rm -rf ${ASSIMP_SRC}

# --------------------------------------------------------------------------------
# Install libwebsockets (2.0 from Debian is not reliable)
# https://github.com/warmcat/libwebsockets/releases
# --------------------------------------------------------------------------------
ARG LWS_VERSION=2.3.0
ARG LWS_SRC=/app/libwebsockets
ARG LWS_FILE=v${LWS_VERSION}.tar.gz

RUN mkdir -p ${LWS_SRC} \
    && wget --no-verbose https://github.com/warmcat/libwebsockets/archive/${LWS_FILE} \
    && tar zxvf ${LWS_FILE} -C ${LWS_SRC} --strip-components=1 \
    && cd ${LWS_SRC} \
    && mkdir -p build \
    && cd build \
    && cmake .. -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLWS_STATIC_PIC=ON \
    -DLWS_WITH_SSL=OFF \
    -DLWS_WITH_ZLIB=OFF \
    -DLWS_WITH_ZIP_FOPS=OFF \
    -DLWS_WITHOUT_EXTENSIONS=ON \
    -DLWS_WITHOUT_TESTAPPS=ON \
    -DLWS_WITH_LIBUV=ON \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} \
    && ninja install
# RUN rm -rf ${LWS_SRC}

# --------------------------------------------------------------------------------
# Install Rockets
# https://github.com/BlueBrain/Rockets
# --------------------------------------------------------------------------------
ARG ROCKETS_VERSION=1.0.0
ARG ROCKETS_SRC=/app/rockets

RUN mkdir -p ${ROCKETS_SRC} \
    && git clone https://github.com/BlueBrain/Rockets ${ROCKETS_SRC}

RUN cd ${ROCKETS_SRC} \
    && git -c advice.detachedHead=false checkout ${ROCKETS_VERSION} \
    && git submodule update --init --recursive \
    && mkdir -p build \
    && cd build \
    && cmake .. -Wno-return-type \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} || exit 0

RUN cd ${ROCKETS_SRC}/build && make -j install
# RUN rm -rf ${ROCKETS_SRC}

# --------------------------------------------------------------------------------
# Get ISPC
# https://github.com/ispc/ispc/releases/download/v1.10.0/ispc-v1.10.0b-linux.tar.gz
# --------------------------------------------------------------------------------
ARG ISPC_VERSION=1.10.0
ARG ISPC_DIR=ispc-v${ISPC_VERSION}b-linux
ARG ISPC_PATH=/app/$ISPC_DIR

RUN mkdir -p ${ISPC_PATH} \
    && wget --no-verbose https://github.com/ispc/ispc/releases/download/v${ISPC_VERSION}/${ISPC_DIR}.tar.gz \
    && tar zxvf ${ISPC_DIR}.tar.gz -C ${ISPC_PATH} --strip-components=1 \
    && rm -rf ${ISPC_PATH}/${ISPC_DIR}/examples

# Add ispc bin to the PATH
ENV PATH $PATH:${ISPC_PATH}/bin

# --------------------------------------------------------------------------------
# Install embree
# https://github.com/embree/embree/releases
# --------------------------------------------------------------------------------
ARG EMBREE_VERSION=3.5.2
ARG EMBREE_FILE=embree-${EMBREE_VERSION}.x86_64.linux.tar.gz
RUN mkdir -p ${DIST_PATH} \
    && wget --no-verbose https://github.com/embree/embree/releases/download/v${EMBREE_VERSION}/${EMBREE_FILE} \
    && tar zxvf ${EMBREE_FILE} -C ${DIST_PATH} --strip-components=1 \
    && rm -rf ${DIST_PATH}/bin ${DIST_PATH}/doc

# --------------------------------------------------------------------------------
# Install OSPRay
# https://github.com/ospray/ospray/releases
# --------------------------------------------------------------------------------
ARG OSPRAY_TAG=v1.8.5
ARG OSPRAY_SRC=/app/ospray

RUN mkdir -p ${OSPRAY_SRC} \
    && git clone https://github.com/ospray/ospray.git ${OSPRAY_SRC} \
    && cd ${OSPRAY_SRC} \
    && git -c advice.detachedHead=false checkout ${OSPRAY_TAG} \
    && mkdir -p build \
    && cd build \
    && CMAKE_PREFIX_PATH=${DIST_PATH} cmake .. -GNinja \
    -DOSPRAY_ENABLE_TUTORIALS=OFF \
    -DOSPRAY_ENABLE_APPS=OFF \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} \
    && ninja install
# RUN rm -rf ${OSPRAY_SRC}

# --------------------------------------------------------------------------------
# Install Brayns
# https://github.com/favreau/Brayns
# --------------------------------------------------------------------------------
ARG BRAYNS_TAG=master
ARG BRAYNS_SRC=/app/brayns

RUN mkdir -p ${BRAYNS_SRC} \
    && git clone https://github.com/favreau/Brayns ${BRAYNS_SRC}

RUN cksum ${BRAYNS_SRC}/.gitsubprojects \
    && cd ${BRAYNS_SRC} \
    && git -c advice.detachedHead=false checkout ${BRAYNS_TAG} \
    && git submodule update --init --recursive \
    && mkdir -p build \
    && cd build \
    && CMAKE_PREFIX_PATH=${DIST_PATH}:${DIST_PATH}/lib/cmake/libwebsockets \
    cmake .. -Wno-dev -Wno-maybe-uninitialized \
    -DBRAYNS_BBIC_ENABLED=OFF \
    -DBRAYNS_BENCHMARK_ENABLED=OFF \
    -DBRAYNS_CIRCUITEXPLORER_ENABLED=OFF \
    -DBRAYNS_CIRCUITRENDERER_ENABLED=OFF \
    -DBRAYNS_CIRCUITINFO_ENABLED=OFF \
    -DBRAYNS_CIRCUITVIEWER_ENABLED=OFF \
    -DBRAYNS_DEFLECT_ENABLED=OFF \
    -DBRAYNS_DTI_ENABLED=OFF \
    -DBRAYNS_IBL_ENABLED=OFF \
    -DBRAYNS_MULTIVIEW_ENABLED=OFF \
    -DBRAYNS_OPENDECK_ENABLED=OFF \
    -DBRAYNS_OPTIX_ENABLED=OFF \
    -DBRAYNS_UNIT_TESTING_ENABLED=OFF \
    -DBRAYNS_VIEWER_ENABLED=OFF \
    -DBRAYNS_ASSIMP_ENABLED=ON \
    -DBRAYNS_OSPRAY_ENABLED=ON \
    -DBRAYNS_NETWORKING_ENABLED=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} || exit 0

RUN cd ${BRAYNS_SRC}/build && make -j install
# RUN rm -rf ${BRAYNS_SRC}

# --------------------------------------------------------------------------------
# Install BioExplorer
# https://github.com/BlueBrain/BioExplorer
# --------------------------------------------------------------------------------
ARG BIOEXPLORER_TAG=1.1.1
ARG BIOEXPLORER_SRC=/app/bioexplorer

RUN mkdir -p ${BIOEXPLORER_SRC} \
    && git clone https://github.com/BlueBrain/BioExplorer.git ${BIOEXPLORER_SRC} \
    && cd ${BIOEXPLORER_SRC} \
    && git -c advice.detachedHead=false checkout ${BIOEXPLORER_TAG} \
    && mkdir -p build \
    && cd build \
    && CMAKE_PREFIX_PATH=${DIST_PATH} cmake .. -Wno-dev \
    -DCGAL_DO_NOT_WARN_ABOUT_CMAKE_BUILD_TYPE=TRUE \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} || exit 0

RUN cd ${BIOEXPLORER_SRC}/build && make -j install
# RUN rm -rf ${BIOEXPLORER_SRC}

# --------------------------------------------------------------------------------
# Install Brion 
# https://github.com/BlueBrain/Brion
# --------------------------------------------------------------------------------
# ARG BRION_TAG=3.3.4
# ARG BRION_SRC=/app/brion

# RUN rm -rf ${BRION_SRC} \
#     && mkdir -p ${BRION_SRC} \
#     && cd ${BRION_SRC} \
#     && git clone https://github.com/BlueBrain/Brion ${BRION_SRC}

# RUN cd ${BRION_SRC} \
#     && git -c advice.detachedHead=false checkout ${BRION_TAG} \
#     && git submodule update --init --recursive \
#     && mkdir -p build \
#     && cd build \
#     && cmake .. -Wno-dev \
#     -DEXTLIB_FROM_SUBMODULES=ON\
#     -DCMAKE_BUILD_TYPE=Release \
#     -DCMAKE_INSTALL_PREFIX=${DIST_PATH} || exit 0

# RUN cd ${BRION_SRC}/build && make -j install
# RUN rm -rf ${BRION_SRC}

# --------------------------------------------------------------------------------
# Install HighFive 
# Note: Brion does not install HighFive, that's why we need to compile 
# and install it
# https://github.com/BlueBrain/HighFive
# --------------------------------------------------------------------------------
ARG HIGHFIVE_VERSION=v2.3.1
ARG HIGHFIVE_SRC=/app/highfive

RUN mkdir -p ${HIGHFIVE_SRC} \
    && git clone https://github.com/BlueBrain/HighFive ${HIGHFIVE_SRC}

RUN cd ${HIGHFIVE_SRC} \
    && git -c advice.detachedHead=false checkout ${HIGHFIVE_VERSION} \
    && git submodule update --init --recursive \
    && rm -rf build \
    && mkdir -p build \
    && cd build \
    && cmake .. -Wno-return-type \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} || exit 0

RUN cd ${HIGHFIVE_SRC}/build && make -j install
# RUN rm -rf ${HIGHFIVE_SRC}

#--------------------------------------------------------------------------------
# Add CircuitExplorer
#--------------------------------------------------------------------------------
ARG CIRCUITEXPLORER_SRC=/app/circuitexplorer
ADD . ${CIRCUITEXPLORER_SRC}

RUN rm -rf ${CIRCUITEXPLORER_SRC}/docker \ 
    && mkdir -p ${CIRCUITEXPLORER_SRC}/docker \
    && cd ${CIRCUITEXPLORER_SRC}/docker \
    && CMAKE_PREFIX_PATH=${DIST_PATH} cmake .. -Wno-dev \
    -DCIRCUITEXPLORER_USE_VASCULATURE=ON \
    -DCIRCUITEXPLORER_USE_MORPHOLOGIES=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} || exit 0

RUN cd ${CIRCUITEXPLORER_SRC}/docker && make -j install

# --------------------------------------------------------------------------------
# Final image, containing only Brayns, CircuitExplorer and their dependencies
# --------------------------------------------------------------------------------
# RUN rm -rf ${DIST_PATH}/include ${DIST_PATH}/cmake ${DIST_PATH}/share

FROM ubuntu:20.04
ARG DIST_PATH=/app/dist

RUN apt-get update \
    && apt-get -y --no-install-recommends install \
    libarchive13 \
    libboost-filesystem1.71.0 \
    libboost-program-options1.71.0 \
    libboost-regex1.71.0 \
    libboost-serialization1.71.0 \
    libboost-system1.71.0 \
    libboost-iostreams1.71.0 \
    libfreeimage3 \
    libgomp1 \
    libhdf5-103 \
    libturbojpeg \
    libuv1 \
    libcgal-dev \
    libpqxx-6.4 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# --------------------------------------------------------------------------------
# The COPY command below will:
# 1. create a container based on the `builder` image (but do not start it)
#    Equivalent to the `docker create` command
# 2. create a new image layer containing the
#    /app/dist directory of this new container
#    Equivalent to the `docker copy` command.
# --------------------------------------------------------------------------------
COPY --from=builder ${DIST_PATH} ${DIST_PATH}

# --------------------------------------------------------------------------------
# Add binaries from dist to the PATH
# --------------------------------------------------------------------------------
ENV LD_LIBRARY_PATH $LD_LIBRARY_PATH:${DIST_PATH}/lib
ENV PATH ${DIST_PATH}/bin:$PATH

# --------------------------------------------------------------------------------
# Expose a port from the container
# For more ports, use the `--expose` flag when running the container,
# see https://docs.docker.com/engine/reference/run/#expose-incoming-ports for docs.
# --------------------------------------------------------------------------------
EXPOSE 8200

# --------------------------------------------------------------------------------
# When running `docker run -ti --rm -p 8200:8200 CircuitExplorer`,
# this will be the cmd that will be executed (+ the CLI options from CMD).
# To ssh into the container (or override the default entry) use:
# `docker run -ti --rm --entrypoint bash -p 8200:8200 CircuitExplorer`
# See https://docs.docker.com/engine/reference/run/#entrypoint-default-command-to-execute-at-runtime
# for more docs
# --------------------------------------------------------------------------------
ENTRYPOINT ["braynsService"]
CMD ["--http-server", ":8200", "--plugin", "CircuitExplorer", "--plugin", "BioExplorer", "--plugin", "MediaMaker"]
