# Open Tasks

One line per task, newest on top: date, status, who asked. An entry is removed when the task is implemented, entered in [FEATURES.md](FEATURES.md), tested green and entered in [TESTS.md](TESTS.md).

## Maintainer orders

- 2026-10-30, open, decision of Marc: the stage `server` is `FROM redis:alpine`, a foreign base image in a build stage, because the Alpine package cannot load the snapshots of `redis:latest` (F10)
