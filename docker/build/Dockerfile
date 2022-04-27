FROM debian:buster
MAINTAINER Jarle Aase <jgaa@jgaa.com>

RUN DEBIAN_FRONTEND="noninteractive" apt-get -q update &&\
    DEBIAN_FRONTEND="noninteractive" apt-get -y -q --no-install-recommends upgrade && \
    DEBIAN_FRONTEND="noninteractive" apt-get install -y -q \
        coreutils git cmake build-essential libssl-dev \
        libzstd-dev libboost-all-dev g++ zlib1g-dev


