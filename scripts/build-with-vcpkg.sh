#!/bin/bash

die() {
    echo "$*" 1>&2
    exit 1;
}

project=shinysocks

if [ -z ${BUILD_DIR+x} ]; then
    echo "Need the envvar VCPKG_DIR to point to where vcpkg is installed"
    exit 1
fi

if [ -z ${BUILD_DIR+x} ]; then
    BUILD_DIR="$(pwd)/build"
else
    BUILD_DIR="${BUILD_DIR}/${project}-build"
fi

echo "Building ${project} in path: ${BUILD_DIR}"

if test -d ${BUILD_DIR}; then
  echo "Cleaning the build directory..."
  rm -rf ${BUILD_DIR}/*
else
    mkdir -p ${BUILD_DIR} || die "Failed to create directory ${BUILD_DIR}"
fi

# pushd ${BUILD_DIR}
#
# ${VCPKG_DIR}/bootstrap-vcpkg.sh -disableMetrics
# vcpkg install || die "vcpkg install failed"
#
# popd

cmake -B ${BUILD_DIR} -S . "-DCMAKE_TOOLCHAIN_FILE=${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake" || die "CMake failed"

pushd ${BUILD_DIR}

cmake --build . || die "Build failed"

popd
