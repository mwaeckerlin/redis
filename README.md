# Redis

Headless, shell-free Redis container built on `mwaeckerlin/scratch`. Only `redis-server` + its shared libraries. **No shell, no perl, no busybox, no `redis-cli`, no package manager.** Everything an attacker who reaches code execution inside the container would need to pivot is not there — this is the security pitch over `redis:alpine` (which ships busybox, apk-tools, and the full redis-* toolset).

Intended primary use inside `mwaeckerlin/mailservice` as the backend for Rspamd's Bayes classifier, greylist state, ratelimit tokens and fuzzy caches. Works standalone against any application that speaks the RESP protocol on port 6379.

## Environment variables

| Variable                | Default                          | Description                                                                                                                                                                                    |
|-------------------------|----------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `REDIS_BIND`            | `0.0.0.0`                        | Interface(s) to bind. Default is «all» because the container is meant to be reachable from other containers on the same Docker network; the network itself is the security boundary.           |
| `REDIS_PORT`            | `6379`                           |                                                                                                                                                                                                |
| `REDIS_PASSWORD`        | *(empty)*                        | If set, `requirepass` is enabled and clients must `AUTH`. Recommended for shared / less-trusted networks.                                                                                       |
| `REDIS_PASSWORD_FILE`   | *(empty)*                        | Path of a file that holds the password, such as a Docker secret `/run/secrets/redis_password`. The line break at the end of the file is not part of the password. The file must be readable by uid 102. Set either `REDIS_PASSWORD` or `REDIS_PASSWORD_FILE`. |
| `REDIS_APPENDONLY`      | `yes`                            | Enable AOF persistence to `/data/appendonly.aof`. Recommended: `yes` — losing Bayes / greylist state on a container restart is painful (weeks of user training gone).                          |
| `REDIS_SAVE`            | `3600 1 300 100 60 10000`        | RDB snapshot schedule (Redis default). Used alongside AOF for faster restarts.                                                                                                                 |
| `REDIS_MAXMEMORY`       | *(empty)*                        | Memory limit (e.g. `256mb`). **Empty = unlimited, which is the intended default** — no artificial cap, Redis uses as much RAM as its data needs. Bayes/greylist/ratelimit state grows slowly; monitor host RAM and only set a limit if you must bound it. Once a limit is set, `REDIS_MAXMEMORY_POLICY` (`allkeys-lfu`) evicts the least-used keys instead of failing writes. |
| `REDIS_MAXMEMORY_POLICY`| `allkeys-lfu`                    | Always configured; takes effect once a memory limit exists (`REDIS_MAXMEMORY` or runtime `CONFIG SET maxmemory`). `allkeys-lfu` fits the Bayes + greylist + ratelimit mix: least-frequently-used keys are evicted first, keeping hot state alive under pressure. |

Every value is checked before `redis.conf` is written: the password takes printable characters without space, quotes and backslash; the bind list takes IPv4 and IPv6 addresses separated by single spaces; the port is a number from 1 to 65535; `REDIS_APPENDONLY` is `yes` or `no`; `REDIS_SAVE` is pairs of numbers or `""`; `REDIS_MAXMEMORY` is a number with an optional unit `k`, `kb`, `m`, `mb`, `g` or `gb`; the eviction policy is one of the eight policies of Redis. The file of `REDIS_PASSWORD_FILE` must be a regular file, must not be empty and takes the same characters as `REDIS_PASSWORD`; with both variables set the container stops as well. Any other value stops the container with `invalid <VARIABLE>`.

With a Docker secret in a compose file:

```yaml
services:
  redis:
    image: mwaeckerlin/redis
    environment:
      REDIS_PASSWORD_FILE: /run/secrets/redis_password
    secrets:
      - redis_password

secrets:
  redis_password:
    file: ./redis_password.txt
```

## Ports

Redis listens on port `6379` inside the container. The compose file of this repository publishes it on every address of the host, IPv4 and IPv6, as long as `BIND_ADDRESS` is unset or empty. `BIND_ADDRESS` limits it to one address of the host: `127.0.0.1` serves the local machine alone, an IPv6 address stands in brackets, such as `[::1]`. `0.0.0.0` publishes on every IPv4 address and on no IPv6 address. A stack whose clients reach Redis over the Docker network publishes no port at all.

## Volumes to persist

- `/data` — Redis RDB snapshots and (if `REDIS_APPENDONLY=yes`) the AOF log. **Persist this volume**: losing it wipes Bayes training, greylist whitelist state and any ratelimit windows. The server runs as user `redis`, uid 102; a host directory or a pre-filled volume belongs to that uid.

## Health check

The image carries a `HEALTHCHECK` (every 30s, timeout 5s, three retries): `/usr/bin/init --healthcheck` sends `AUTH` with the password of `REDIS_PASSWORD` or `REDIS_PASSWORD_FILE` when one is set, then `PING`, and reports healthy only on `PONG`. A compose file or a Swarm stack therefore needs no healthcheck of its own; `depends_on: condition: service_healthy` works as it is. To check by hand:

```bash
docker compose exec redis /usr/bin/init --healthcheck
```

## Client access from a peer container

The container carries neither `redis-cli` nor a shell, so the client runs in another container or on the host:

```bash
docker run --rm --network <your-net> redis:alpine redis-cli -h redis ping
```

If your primary consumer (e.g. `mwaeckerlin/rspamd`) needs to run diagnostics, install `redis-cli` there, or exec into the peer:

```bash
docker compose exec rspamd rspamc stat   # queries Rspamd, which in turn hits Redis
```

## Testing

```bash
npm test
```

runs five suites and fails on any single error:

1. **Docs contract** (`tests/docs-contract.sh`) — every feature of [FEATURES.md](FEATURES.md) has a test in [TESTS.md](TESTS.md).
2. **Compose contract** (`tests/compose-contract.sh`) — every port published in `docker-compose.yml` lies on every address of the host unless `BIND_ADDRESS` names one.
3. **Image contract** (`tests/image-contract.sh`) — the image must be headless: no `/bin/sh`, no bash, no busybox, no perl.
4. **Config validation** (`tests/config-validation.sh`) — every malformed value is refused, every valid one accepted.
5. **Functional e2e** (`tests/run.sh`, via `tests/docker-compose.yml`)&nbsp;— PING, SET/GET, AOF persistence across a container restart, `REDIS_PASSWORD` enforcement (NOAUTH without, PONG with), the password of `REDIS_PASSWORD_FILE` from a Docker secret, the `allkeys-lfu` default eviction policy, the image's health check (healthy without a compose healthcheck, refused with a wrong password and against a port nobody serves) the uid 102 of the user `redis`, that a `dump.rdb` written by the current `redis:latest` loads, and that the server has the version of `redis:alpine`. The official `redis:alpine` image serves purely as the `redis-cli` client — the image under test ships none on purpose.

## Image layout

The image is built in the same stages as `mwaeckerlin/nginx`, `mwaeckerlin/php-fpm`, `mwaeckerlin/opendkim`, `mwaeckerlin/opendmarc` and `mwaeckerlin/rspamd`:

1. **`init`** — compiles `init.cpp` statically with `g++ -static -Os -flto`. The resulting binary reads env, composes `/run/redis/redis.conf` and `execv`s `redis-server`.
2. **`build`** — creates the user `redis` (uid 102), `/data` and `/run/redis` on the Alpine base and takes `redis-server` with its `.so` dependencies from the stage `server`: `FROM redis:alpine`, the official release that `redis:latest` also carries, built on musl. The Alpine package lags behind the official release and cannot load the snapshots a current `redis:latest` writes; the weekly rebuild takes each new release.
3. **runtime** — `FROM mwaeckerlin/scratch`, `COPY --from=build /root/ /`. Runtime image is a few MB. `ENTRYPOINT ["/usr/bin/init"]`.

The building blocks, the runs and the deployment are described in [ARCHITECTURE.md](ARCHITECTURE.md).
