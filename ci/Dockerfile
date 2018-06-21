FROM ubuntu:xenial

MAINTAINER Jarle Aase <jgaa@jgaa.com>

# In case you need proxy
RUN apt-get -q update &&\
   apt-get -y -q --no-install-recommends upgrade &&\
   apt-get -y -q install libboost-system1.58.0 libboost-program-options1.58.0 \
       libboost-filesystem1.58.0 libboost-coroutine1.58.0 libboost-log1.58.0 \
       libboost-thread1.58.0 libboost-context1.58.0 && \
   apt-get -y -q autoremove &&\
   apt-get -y -q clean &&\
   mkdir /etc/shinysocks

COPY ci/shinysocks.conf /etc/shinysocks
COPY build/shinysocks /usr/bin/shinysocks

# Standard SOCKS port
EXPOSE 1080

USER nobody

# Default command
CMD ["/usr/bin/shinysocks", "-c", "/etc/shinysocks/shinysocks.conf"]
