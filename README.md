# Redis

Headless, shell-free Redis container built on `mwaeckerlin/scratch`. Only `redis-server` + its shared libraries. **No shell, no perl, no busybox, no `redis-cli`, no package manager.** Everything an attacker who reaches code execution inside the container would need to pivot is not there — this is the security pitch over `redis:alpine` (which ships busybox, apk-tools, and the full redis-* toolset).

Intended primary use inside `mwaeckerlin/mailservice` as the backend for Rspamd's Bayes classifier, greylist state, ratelimit tokens and fuzzy caches. Works standalone against any application that speaks the RESP protocol on port 6379.

## Environment variables

| Variable                | Default                          | Description                                                                                                                                                                                    |
|-------------------------|----------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `REDIS_BIND`            | `0.0.0.0`                        | Interface(s) to bind. Default is «all» because the container is meant to be reachable from other containers on the same Docker network; the network itself is the security boundary.           |
| `REDIS_PORT`            | `6379`                           |                                                                                                                                                                                                |
| `REDIS_PASSWORD`        | *(empty)*                        | If set, `requirepass` is enabled and clients must `AUTH`. Recommended for shared / less-trusted networks.                                                                                       |
| `REDIS_APPENDONLY`      | `yes`                            | Enable AOF persistence to `/data/appendonly.aof`. Recommended: `yes` — losing Bayes / greylist state on a container restart is painful (weeks of user training gone).                          |
| `REDIS_SAVE`            | `3600 1 300 100 60 10000`        | RDB snapshot schedule (Redis default). Used alongside AOF for faster restarts.                                                                                                                 |
| `REDIS_MAXMEMORY`       | *(empty)*                        | Memory limit (e.g. `256mb`). **Empty = unlimited, which is the intended default** — no artificial cap, Redis uses as much RAM as its data needs. Bayes/greylist/ratelimit state grows slowly; monitor host RAM and only set a limit if you must bound it. Once a limit is set, `REDIS_MAXMEMORY_POLICY` (`allkeys-lfu`) evicts the least-used keys instead of failing writes. |
| `REDIS_MAXMEMORY_POLICY`| `allkeys-lfu`                    | Always configured; takes effect once a memory limit exists (`REDIS_MAXMEMORY` or runtime `CONFIG SET maxmemory`). `allkeys-lfu` fits the Bayes + greylist + ratelimit mix: least-frequently-used keys are evicted first, keeping hot state alive under pressure. |

Every value is checked before `redis.conf` is written: the password takes printable characters without space, quotes and backslash; the bind list takes IPv4 and IPv6 addresses separated by single spaces; the port is a number from 1 to 65535; `REDIS_APPENDONLY` is `yes` or `no`; `REDIS_SAVE` is pairs of numbers or `""`; `REDIS_MAXMEMORY` is a number with an optional unit `k`, `kb`, `m`, `mb`, `g` or `gb`; the eviction policy is one of the eight policies of Redis. Any other value stops the container with `invalid <VARIABLE>`.

## Volumes to persist

- `/data` — Redis RDB snapshots and (if `REDIS_APPENDONLY=yes`) the AOF log. **Persist this volume**: losing it wipes Bayes training, greylist whitelist state and any ratelimit windows.

## Client access from a peer container

The container carries neither `redis-cli` nor a shell, so the client runs in another container or on the host:

```bash
$ docker run --rm --network <your-net> redis:alpine redis-cli -h redis ping
```

If your primary consumer (e.g. `mwaeckerlin/rspamd`) needs to run diagnostics, install `redis-cli` there, or exec into the peer:

```bash
$ docker compose exec rspamd rspamc stat   # queries Rspamd, which in turn hits Redis
```

## Testing

```bash
$ npm test
```

runs four suites and fails on any single error:

1. **Docs contract** (`tests/docs-contract.sh`) — every feature of [FEATURES.md](FEATURES.md) has a test in [TESTS.md](TESTS.md).
2. **Image contract** (`tests/image-contract.sh`) — the image must be headless: no `/bin/sh`, no bash, no busybox, no perl.
3. **Config validation** (`tests/config-validation.sh`) — every malformed value is refused, every valid one accepted.
4. **Functional e2e** (`tests/run.sh`, via `tests/docker-compose.yml`)&nbsp;— PING, SET/GET, AOF persistence across a container restart, `REDIS_PASSWORD` enforcement (NOAUTH without, PONG with), and the `allkeys-lfu` default eviction policy. The official `redis:alpine` image serves purely as the `redis-cli` client — the image under test ships none on purpose.

## Image layout

The image is built in the same stages as `mwaeckerlin/nginx`, `mwaeckerlin/php-fpm`, `mwaeckerlin/opendkim`, `mwaeckerlin/opendmarc` and `mwaeckerlin/rspamd`:

1. **`init`** — compiles `init.cpp` statically with `g++ -static -Os -flto`. The resulting binary reads env, composes `/run/redis/redis.conf` and `execv`s `redis-server`.
2. **`build`** — installs `redis` on the Alpine base, then uses `tar cph … + ldd` to collect only `redis-server` and its `.so` dependencies into `/root/`.
3. **runtime** — `FROM mwaeckerlin/scratch`, `COPY --from=build /root/ /`. Runtime image is a few MB. `ENTRYPOINT ["/usr/bin/init"]`.
