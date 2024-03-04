#!/bin/bash

# Build shinysocks using the build image, and then copy
# the binary and boost to the artifacts location.

die() {
    echo "$*" 1>&2
    exit 1;
}

echo "Building shinysocks..."

cd /build || die

cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} /src || die "CMake failed configure step"

make -j `nproc` || die "Build failed"

if ${DO_STRIP} ; then
    echo "Stripping binary"
    strip bin/*
fi

cp -v bin/* /artifacts/bin
cp -v /src/docker/shinysocks.conf /artifacts
cp -v /src/docker/logbt.sh /src/docker/startup.sh /artifacts
