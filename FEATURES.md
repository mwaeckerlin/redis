# Features

Numbered register of every feature; a number is never reused. Every feature is covered by tests listed in [TESTS.md](TESTS.md); the guard `tests/docs-contract.sh` fails when a feature has no test.

- **F1 — Redis server on port 6379.** `redis-server` answers the RESP protocol on `REDIS_PORT` (default 6379), bound to `REDIS_BIND` (default `0.0.0.0`, all interfaces, because the Docker network is the boundary).
- **F2 — Durable by default.** `REDIS_APPENDONLY=yes` (default) writes every change to `/data/appendonly.aof`, so the data survives a container restart; `REDIS_SAVE` sets the snapshot schedule (default `3600 1 300 100 60 10000`, `""` switches snapshots off).
- **F3 — Password.** With `REDIS_PASSWORD` set, every client has to authenticate; a command without `AUTH` is refused.
- **F4 — Memory limit and eviction.** `REDIS_MAXMEMORY` limits the memory (empty: no limit); `REDIS_MAXMEMORY_POLICY` (default `allkeys-lfu`) decides what is evicted once a limit is reached.
- **F5 — Validated configuration.** Every variable is checked against what its directive accepts before `redis.conf` is written; a malformed value, above all one with a line break that would add directives of its own, stops the container with `invalid <VARIABLE>`.
- **F6 — Headless.** Only `redis-server`, its libraries and the start program are in the image: no shell, no `redis-cli`, no package manager.
- **F8 — Health check in the image.** The image carries a `HEALTHCHECK`: `/usr/bin/init --healthcheck` sends `AUTH` (when `REDIS_PASSWORD` is set) and `PING` to `127.0.0.1:REDIS_PORT` and reports healthy only on the reply `PONG`. A wrong password or a port nobody serves reports unhealthy. A compose file or a Swarm stack needs no healthcheck of its own.
- **F9 — Fixed user.** The server runs as user `redis`, uid 102, so a deployment can hand `/data` to that uid.
- **F10 — Snapshots of the official image.** The server is the release of the official `redis:alpine`, the same as `redis:latest`, so a `/data` with a `dump.rdb` that `redis:latest` wrote is loaded; the weekly rebuild takes each new release.
- **F7 — Published for amd64 and arm64.** Every push builds the image natively for both architectures and publishes it under one tag on Docker Hub, with the reusable workflow of `mwaeckerlin/scratch`.
