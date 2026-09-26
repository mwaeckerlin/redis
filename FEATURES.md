# Features

Numbered register of every feature; a number is never reused. Every feature is covered by tests listed in [TESTS.md](TESTS.md); the guard `tests/docs-contract.sh` fails when a feature has no test.

- **F1 — Redis server on port 6379.** `redis-server` answers the RESP protocol on `REDIS_PORT` (default 6379), bound to `REDIS_BIND` (default `0.0.0.0`, all interfaces, because the Docker network is the boundary).
- **F2 — Durable by default.** `REDIS_APPENDONLY=yes` (default) writes every change to `/data/appendonly.aof`, so the data survives a container restart; `REDIS_SAVE` sets the snapshot schedule (default `3600 1 300 100 60 10000`, `""` switches snapshots off).
- **F3 — Password.** With `REDIS_PASSWORD` set, every client has to authenticate; a command without `AUTH` is refused.
- **F4 — Memory limit and eviction.** `REDIS_MAXMEMORY` limits the memory (empty: no limit); `REDIS_MAXMEMORY_POLICY` (default `allkeys-lfu`) decides what is evicted once a limit is reached.
- **F5 — Validated configuration.** Every variable is checked against what its directive accepts before `redis.conf` is written; a malformed value, above all one with a line break that would add directives of its own, stops the container with `invalid <VARIABLE>`.
- **F6 — Headless.** Only `redis-server`, its libraries and the start program are in the image: no shell, no `redis-cli`, no package manager.
- **F7 — Published for amd64 and arm64.** Every push builds the image natively for both architectures and publishes it under one tag on Docker Hub, with the reusable workflow of `mwaeckerlin/scratch`.
