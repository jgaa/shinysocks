#!/bin/bash

echo "Building docker image for shinysocks"

pushd docker/build
docker build -t shinysocks-bld .
popd

root=$(pwd)
docker run -u $UID --rm -v `pwd`:/src shinysocks-bld bash -c 'mkdir /tmp/build && cd /tmp/build && cmake -DCMAKE_BUILD_TYPE=Release /src && make -j `nproc` && cp shinysocks /src/docker/target'
ls 

cp -v ci/shinysocks.conf docker/target

pushd docker/target
docker build -t jgaafromnorth/shinysocks .
echo rm -v shinysocks.conf shinysocks 
popd
