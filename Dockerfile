FROM mwaeckerlin/very-base AS init
RUN $PKG_INSTALL g++
COPY init.cpp .
RUN g++ -static -Os -flto=auto -fno-rtti -ffunction-sections -fdata-sections \
        -Wl,--gc-sections -Wl,-s -std=c++20 -o init init.cpp
RUN strip -s -R .comment -R .gnu.version --strip-unneeded init

FROM mwaeckerlin/very-base AS build
RUN $PKG_INSTALL redis
RUN mkdir -p /data /run/redis \
    && chown -R redis:redis /data /run/redis
COPY --from=init init /usr/bin/init

# Collect only redis-server + its shared libs into /root/. The scratch
# runtime image contains no shell, no perl, no busybox, and — beyond
# redis-server itself — no other redis-* helpers. That means no
# redis-cli either; use `docker compose exec` from a peer container
# with the client, or connect over the exposed port.
RUN tar cph \
        /data /run/redis \
        /etc/passwd /etc/group /etc/nsswitch.conf \
        /usr/bin/redis-server /usr/bin/init \
        $(ldd /usr/bin/redis-server | sed -n 's,.* => \([^ ]*\) .*,\1,p') \
    | tar xpC /root/

FROM mwaeckerlin/scratch
ENV CONTAINERNAME="redis" \
    REDIS_BIND="0.0.0.0" \
    REDIS_PORT="6379" \
    REDIS_APPENDONLY="yes" \
    REDIS_SAVE="3600 1 300 100 60 10000" \
    REDIS_PASSWORD="" \
    REDIS_MAXMEMORY="" \
    REDIS_MAXMEMORY_POLICY="allkeys-lfu"
EXPOSE 6379
VOLUME /data
USER redis
ENTRYPOINT ["/usr/bin/init"]
COPY --from=build /root/ /
