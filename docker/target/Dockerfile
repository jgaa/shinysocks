FROM debian:buster
MAINTAINER Jarle Aase <jgaa@jgaa.com>

# In case you need proxy
RUN apt-get -q update &&\
   apt-get -y -q --no-install-recommends upgrade &&\
   apt-get -y -q install libboost-system1.67.0 libboost-program-options1.67.0 \
       libboost-filesystem1.67.0 libboost-coroutine1.67.0 libboost-log1.67.0 \
       libboost-thread1.67.0 libboost-context1.67.0 libboost-serialization1.67.0 \
       libboost-iostreams1.67.0 && \
   apt-get -y -q autoremove &&\
   apt-get -y -q clean &&\
   mkdir /etc/shinysocks

COPY shinysocks.conf /etc/shinysocks
COPY shinysocks /usr/bin/shinysocks

# Standard SOCKS port
EXPOSE 1080

USER nobody

# Default command
CMD ["/usr/bin/shinysocks", "-c", "/etc/shinysocks/shinysocks.conf"]
