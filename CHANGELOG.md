# Changelog

- 2026-07-17 **1.0.0**
    - Initial release: headless, shell-free Redis container
        - runtime contains only the Redis server and its libraries — no shell, no busybox, no client tools, no package manager
        - configuration through environment variables (bind, port, password, persistence, memory limit and eviction policy)
        - durable by default: append-only persistence keeps all stored data across container restarts
        - eviction policy defaults to keeping the most frequently used data when a memory limit is reached
    - Test suite (`npm test`)
        - image contract: the shipped image must be headless
        - functional end-to-end: liveness, store/read, persistence across a restart, password enforcement, default eviction policy
