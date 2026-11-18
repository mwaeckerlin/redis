# Tests

Register of all tests, sorted by the [FEATURES.md](FEATURES.md) number each test covers. `npm test` runs everything; the guard `tests/docs-contract.sh` fails when a feature has no test entry here.

## E2E (`tests/run.sh` against `tests/docker-compose.yml`)

- **F1** `tests/run.sh` › ping, set_get — the server answers and stores and returns a value.
- **F2** `tests/run.sh` › aof_persistence_across_restart — a value survives a container restart.
- **F3** `tests/run.sh` › auth_required_without_password, auth_with_password — a command without `AUTH` is refused, the password works.
- **F12** `tests/run.sh` › auth_with_password_file, password_file_required_without_password, healthcheck_with_password_file, reject_empty_password_file — a server with `REDIS_PASSWORD_FILE` on a Docker secret whose content ends in a line break accepts the password without the line break, refuses a command without `AUTH`, and turns healthy from the image's `HEALTHCHECK`; a secret with nothing but a line break stops the container with `is empty`.
- **F10** `tests/run.sh` › server_is_release_of_redis_alpine — `redis_version` of `INFO server` equals the version of `redis-server --version` in `redis:alpine`.
- **F4** `tests/run.sh` › maxmemory_policy_default — the eviction policy `allkeys-lfu` reaches the server.
- **F8** `tests/run.sh` › image_healthcheck_healthy, healthcheck_with_password, healthcheck_refuses_wrong_password, healthcheck_refuses_closed_port — a service without a compose healthcheck turns healthy from the image's `HEALTHCHECK`; the check passes with the container's password and fails with a wrong one and against a port nobody serves (regression: a bare TCP connect passed with a wrong password).
- **F10** `tests/run.sh` › loads_snapshot_of_redis_latest — `redis:latest` writes `dump.rdb` into a volume, and the image under test, with `REDIS_SAVE="60 1"` and `REDIS_APPENDONLY=no`, starts on it and returns the stored value (regression: Redis 8.8.0 from the Alpine package stopped with «Can't handle RDB format version 15»).
- **F9** `tests/run.sh` › redis_user_uid_102 — `/etc/passwd` of the image gives the user `redis` uid 102.

## Config validation (`tests/config-validation.sh`)

- **F5** reject_password_injection, reject_password_space, reject_bind_injection, reject_bind_bad, reject_port_not_numeric, reject_port_out_of_range, reject_appendonly_bad, reject_save_injection, reject_save_bad, reject_maxmemory_bad, reject_maxmemory_injection, reject_policy_unknown — every malformed value stops the container with `invalid <VARIABLE>` (regression: a line break in `REDIS_PASSWORD` added a directive to `redis.conf`).
- **F1** accept_explicit_values — `REDIS_BIND='0.0.0.0 ::'` and `REDIS_PORT=6380` pass the validation.
- **F2** accept_explicit_values, accept_save_disabled — `REDIS_APPENDONLY=no`, a snapshot schedule and `REDIS_SAVE='""'` pass.
- **F4** accept_explicit_values — `REDIS_MAXMEMORY=256mb` and `REDIS_MAXMEMORY_POLICY=volatile-lru` pass.
- **F5** accept_defaults — the defaults pass.

- **F12** reject_password_file_missing, reject_password_file_content, reject_password_file_directory, reject_password_file_device, reject_password_and_password_file — a missing file, a file whose content holds spaces and line breaks, the directory `/data`, the device `/dev/zero`, and `REDIS_PASSWORD` beside `REDIS_PASSWORD_FILE` stop the container with `invalid REDIS_PASSWORD_FILE`.

## Compose contract (`tests/compose-contract.sh`)

- **F11** docker-compose_ports_bind_address, selftest_fixed_address_detected — every port published in `docker-compose.yml` starts with `${BIND_ADDRESS:+${BIND_ADDRESS:-}:}`, and an entry without it or with a fixed default address is detected.
- **F11** docker-compose_rendered_every_address, docker-compose_renders_without_warning, docker-compose_rendered_bind_address — `docker compose config` renders without a warning, and it renders every port of `docker-compose.yml` without a host address when `BIND_ADDRESS` is unset or empty, which Docker publishes on every IPv4 and IPv6 address, and with exactly the address that `BIND_ADDRESS` names when it is set.

## Image contract

- **F6** `tests/image-contract.sh` › no sh, no bash, no busybox, no perl — the image is headless.

## Workflow contract

- **F7** `tests/workflow-contract.sh` of `mwaeckerlin/scratch` — the reusable workflow selects exactly the images a repository publishes; this repository calls it from `.github/workflows/docker.yml`.
