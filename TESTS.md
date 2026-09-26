# Tests

Register of all tests, sorted by the [FEATURES.md](FEATURES.md) number each test covers. `npm test` runs everything; the guard `tests/docs-contract.sh` fails when a feature has no test entry here.

## E2E (`tests/run.sh` against `tests/docker-compose.yml`)

- **F1** `tests/run.sh` › ping, set_get — the server answers and stores and returns a value.
- **F2** `tests/run.sh` › aof_persistence_across_restart — a value survives a container restart.
- **F3** `tests/run.sh` › auth_required_without_password, auth_with_password — a command without `AUTH` is refused, the password works.
- **F4** `tests/run.sh` › maxmemory_policy_default — the eviction policy `allkeys-lfu` reaches the server.

## Config validation (`tests/config-validation.sh`)

- **F5** reject_password_injection, reject_password_space, reject_bind_injection, reject_bind_bad, reject_port_not_numeric, reject_port_out_of_range, reject_appendonly_bad, reject_save_injection, reject_save_bad, reject_maxmemory_bad, reject_maxmemory_injection, reject_policy_unknown — every malformed value stops the container with `invalid <VARIABLE>` (regression: a line break in `REDIS_PASSWORD` added a directive to `redis.conf`).
- **F1** accept_explicit_values — `REDIS_BIND='0.0.0.0 ::'` and `REDIS_PORT=6380` pass the validation.
- **F2** accept_explicit_values, accept_save_disabled — `REDIS_APPENDONLY=no`, a snapshot schedule and `REDIS_SAVE='""'` pass.
- **F4** accept_explicit_values — `REDIS_MAXMEMORY=256mb` and `REDIS_MAXMEMORY_POLICY=volatile-lru` pass.
- **F5** accept_defaults — the defaults pass.

## Image contract

- **F6** `tests/image-contract.sh` › no sh, no bash, no busybox, no perl — the image is headless.

## Workflow contract

- **F7** `tests/workflow-contract.sh` of `mwaeckerlin/scratch` — the reusable workflow selects exactly the images a repository publishes; this repository calls it from `.github/workflows/docker.yml`.
