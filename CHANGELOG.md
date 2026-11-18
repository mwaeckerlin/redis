# Changelog

- 2026-11-19 **1.0.5**
    - `REDIS_PASSWORD_FILE` reads the password from a file, such as a Docker secret, at the start and for the health check; the line break at the end of the file is not part of the password
    - Redis 8.10.3, the current release of the official `redis:alpine`

- 2026-10-19 **1.0.4**
    - `BIND_ADDRESS` limits the port that the compose file of this repository publishes to one address of the host, `127.0.0.1` for the local machine alone; without it the port stays on every address, IPv4 and IPv6, as before

- 2026-10-18 **1.0.3**
    - Snapshots that the current official `redis:latest` writes are loaded: the server now comes from the official `redis:alpine` release, where the Alpine package (8.8.0) refused «RDB format version 15»; the weekly rebuild takes each new release with it

- 2026-10-15 **1.0.2**
    - The image reports its health itself: Docker and Swarm show it healthy only when Redis answers `PING` with `PONG`, with the configured password; a wrong password or a server that does not answer shows unhealthy
    - The server user `redis` has uid 102, documented for the permissions of `/data`

- 2026-09-26 **1.0.1**
    - Security: every configuration value from the environment is now validated before it is written into the server configuration; a malformed value, above all a line break in the password that would add directives of its own, stops the container with a clear `invalid <VARIABLE>` error
    - The image is published for amd64 and arm64 under one tag, built and published automatically on every change and every week

- 2026-07-17 **1.0.0**
    - Initial release: headless, shell-free Redis container
        - runtime contains only the Redis server and its libraries — no shell, no busybox, no client tools, no package manager
        - configuration through environment variables (bind, port, password, persistence, memory limit and eviction policy)
        - durable by default: append-only persistence keeps all stored data across container restarts
        - eviction policy defaults to keeping the most frequently used data when a memory limit is reached
    - Test suite (`npm test`)
        - image contract: the shipped image must be headless
        - functional end-to-end: liveness, store/read, persistence across a restart, password enforcement, default eviction policy
