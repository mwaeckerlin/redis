FROM mwaeckerlin/very-base AS init
RUN $PKG_INSTALL g++
COPY init.cpp .
RUN g++ -static -Os -flto=auto -fno-rtti -ffunction-sections -fdata-sections \
        -Wl,--gc-sections -Wl,-s -std=c++20 -o init init.cpp
RUN strip -s -R .comment -R .gnu.version --strip-unneeded init

# The server comes from the official redis:alpine, the same release as
# redis:latest, built on musl: the Alpine package lags behind and cannot
# load the snapshots (RDB format) a current redis:latest writes. The weekly
# rebuild of this image takes the newest release with it. Only redis-server
# and its shared libraries are collected; no redis-cli, no other helpers.
FROM redis:alpine AS server
RUN mkdir -p /out/usr/bin \
    && cp /usr/local/bin/redis-server /out/usr/bin/redis-server \
    && tar ch $(ldd /usr/local/bin/redis-server | sed -n 's,.* => \([^ ]*\) .*,\1,p') \
       | tar x -C /out

FROM mwaeckerlin/very-base AS build
RUN addgroup -S redis \
    && adduser -S -D -H -h /data -G redis redis \
    && mkdir -p /data /run/redis \
    && chown -R redis:redis /data /run/redis
COPY --from=init init /usr/bin/init

# The scratch runtime image contains no shell, no perl, no busybox; use
# `docker compose exec` from a peer container with the client, or connect
# over the exposed port.
RUN tar cph \
        /data /run/redis \
        /etc/passwd /etc/group /etc/nsswitch.conf \
        /usr/bin/init \
    | tar xpC /root/
COPY --from=server /out/ /root/

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
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD ["/usr/bin/init", "--healthcheck"]
ENTRYPOINT ["/usr/bin/init"]
COPY --from=build /root/ /
