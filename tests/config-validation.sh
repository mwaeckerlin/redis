#!/usr/bin/env bash
# Config validation: init must refuse malformed environment values.
#
# Every env knob is written into redis.conf. A value containing a newline
# would smuggle arbitrary extra directives into the configuration (config
# injection, e.g. a second `requirepass` or a `rename-command`); a malformed
# number or keyword would stop redis with an unclear parser error. init
# therefore whitelist-validates every value up front and exits with a clear
# `invalid <VAR>` error before anything else runs.
#
# The image is shell-free, so the checks run from outside: the
# `--healthcheck` entrypoint path performs the same validation first, fails
# fast (no server is running) and never touches the network — `--network
# none` pins that. `--pull=never` keeps docker from testing a stale registry
# image instead of the local build.
#
# Usage: tests/config-validation.sh IMAGE

set -uo pipefail

IMAGE="${1:-mwaeckerlin/redis}"

PASS=0
FAIL=0
declare -a FAILED_NAMES

_pass() { PASS=$((PASS + 1)); echo "  PASS  $1"; }
_fail() { FAIL=$((FAIL + 1)); FAILED_NAMES+=("$1"); echo "  FAIL  $1: $2"; }

_image_exists() {
    if docker image inspect "${IMAGE}" > /dev/null 2>&1; then
        return 0
    fi
    _fail "image_exists" "image not built — run 'npm run build' first"
    return 1
}

# A malformed value must abort with a message naming the variable.
_reject() {
    local name="$1" var="$2" value="$3"
    local out rc
    out=$(timeout 30 docker run --rm --pull=never --network none \
              -e "${var}=${value}" "${IMAGE}" --healthcheck 2>&1)
    rc=$?
    if [[ ${rc} -ne 0 && "${out}" == *"invalid ${var}"* ]]; then
        _pass "reject_${name}"
    else
        _fail "reject_${name}" "value not rejected (rc=${rc}): ${out}"
    fi
}

# A well-formed value must pass validation (the probe itself fails — no
# server is running — but no validation error may appear).
_accept() {
    local name="$1"
    shift
    local out
    out=$(timeout 30 docker run --rm --pull=never --network none \
              "$@" "${IMAGE}" --healthcheck 2>&1)
    if [[ "${out}" == *"invalid "* ]]; then
        _fail "accept_${name}" "valid value rejected: ${out}"
    else
        _pass "accept_${name}"
    fi
}

echo "==> Config validation: malformed environment must be refused"

_image_exists || { echo ""; echo "==> Config validation results: 0 passed, 1 failed"; exit 1; }

_reject password_injection    REDIS_PASSWORD         $'secret\nrename-command CONFIG ""'
_reject password_space        REDIS_PASSWORD         "two words"
_reject bind_injection        REDIS_BIND             $'0.0.0.0\nprotected-mode yes'
_reject bind_bad              REDIS_BIND             "0.0.0.0;rm"
_reject port_not_numeric      REDIS_PORT             "6379x"
_reject port_out_of_range     REDIS_PORT             "70000"
_reject appendonly_bad        REDIS_APPENDONLY       "maybe"
_reject save_injection        REDIS_SAVE             $'3600 1\nappendonly no'
_reject save_bad              REDIS_SAVE             "3600 one"
_reject maxmemory_bad         REDIS_MAXMEMORY        "lots"
_reject maxmemory_injection   REDIS_MAXMEMORY        $'256mb\nmaxclients 1'
_reject policy_unknown        REDIS_MAXMEMORY_POLICY "evict-everything"

_accept defaults
_accept explicit_values \
    -e REDIS_PASSWORD='S3cr3t-with_symbols.!@#%^&*' \
    -e REDIS_BIND='0.0.0.0 ::' \
    -e REDIS_PORT=6380 \
    -e REDIS_APPENDONLY=no \
    -e REDIS_SAVE='900 1 300 10' \
    -e REDIS_MAXMEMORY=256mb \
    -e REDIS_MAXMEMORY_POLICY=volatile-lru
_accept save_disabled -e REDIS_SAVE='""'

echo ""
echo "==> Config validation results: ${PASS} passed, ${FAIL} failed"
if [[ ${FAIL} -gt 0 ]]; then
    echo "==> Failed checks: ${FAILED_NAMES[*]}"
    exit 1
fi
