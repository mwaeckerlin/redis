# Changelog

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
