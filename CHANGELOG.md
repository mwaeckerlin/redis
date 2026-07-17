# Changelog

All notable changes to this project are documented here.

## [Unreleased]

### Added

- Initial release. Headless, shell-free Redis container built on
  `mwaeckerlin/scratch` (three-stage build: statically-linked C++
  `init.cpp` in stage 1, `redis-server` + shared libs collected via
  `tar cph … + ldd` in stage 2, final `FROM mwaeckerlin/scratch`
  in stage 3).
- Runtime image contains **only `redis-server` and its shared
  libraries** — no shell, no perl, no busybox, no `redis-cli`, no
  package manager. Security posture stronger than `redis:alpine`.
- Env-driven config: `REDIS_BIND`, `REDIS_PORT`, `REDIS_PASSWORD`,
  `REDIS_APPENDONLY`, `REDIS_SAVE`, `REDIS_MAXMEMORY`,
  `REDIS_MAXMEMORY_POLICY`. `init.cpp` composes
  `/run/redis/redis.conf` from those before `execv`ing the daemon.
- AOF persistence enabled by default (`REDIS_APPENDONLY=yes`) so
  Bayes / greylist / ratelimit state survives container restarts.
