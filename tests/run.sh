#!/usr/bin/env bash
# Functional e2e tests for mwaeckerlin/redis.
# Usage: bash tests/run.sh
set -euo pipefail

cd "$(dirname "$0")/.."
COMPOSE="tests/docker-compose.yml"

cleanup() {
    docker compose -f "$COMPOSE" down -v --remove-orphans 2>/dev/null || true
}
trap cleanup EXIT

PASS=0
FAIL=0
_pass() { PASS=$((PASS + 1)); echo "  PASS  $1"; }
_fail() { FAIL=$((FAIL + 1)); echo "  FAIL  $1: $2"; }

_cli() { docker compose -f "$COMPOSE" exec -T cli redis-cli "$@"; }

_wait_ping() {
    local host="$1" tries=30
    while (( tries-- > 0 )); do
        if [[ "$(_cli -h "$host" ping 2>/dev/null || true)" == "PONG" ]]; then
            return 0
        fi
        sleep 1
    done
    return 1
}

echo "==> Starting test stack..."
docker compose -f "$COMPOSE" up -d --wait redis redis-auth cli

# 1. liveness
if [[ "$(_cli -h redis ping)" == "PONG" ]]; then
    _pass "ping"
else
    _fail "ping" "no PONG from redis"
fi

# 2. basic data operations
_cli -h redis set e2e-key e2e-value > /dev/null
if [[ "$(_cli -h redis get e2e-key)" == "e2e-value" ]]; then
    _pass "set_get"
else
    _fail "set_get" "stored value not returned"
fi

# 3. persistence across a restart (AOF is on by default; /data volume)
docker compose -f "$COMPOSE" restart redis > /dev/null 2>&1
if _wait_ping redis && [[ "$(_cli -h redis get e2e-key)" == "e2e-value" ]]; then
    _pass "aof_persistence_across_restart"
else
    _fail "aof_persistence_across_restart" "value lost after container restart"
fi

# 4. REDIS_PASSWORD is enforced: unauthenticated commands fail...
if _cli -h redis-auth get e2e-key 2>&1 | grep -q "NOAUTH"; then
    _pass "auth_required_without_password"
else
    _fail "auth_required_without_password" "server accepted a command without AUTH"
fi

# ...and the configured password works.
if [[ "$(_cli -h redis-auth -a e2e-test-password-12 --no-auth-warning ping)" == "PONG" ]]; then
    _pass "auth_with_password"
else
    _fail "auth_with_password" "PING with correct password failed"
fi

# 5. eviction policy from env default reaches the server config
if _cli -h redis config get maxmemory-policy | grep -q "allkeys-lfu"; then
    _pass "maxmemory_policy_default"
else
    _fail "maxmemory_policy_default" "expected allkeys-lfu from image default"
fi

echo ""
echo "==> Functional results: ${PASS} passed, ${FAIL} failed"
[[ ${FAIL} -eq 0 ]] || exit 1
