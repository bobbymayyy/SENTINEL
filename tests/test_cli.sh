#!/bin/sh
set -eu

./sentinel --version | grep -q '^SENTINEL 0\.2\.0$'
./ir-sentinel --version | grep -q '^SENTINEL 0\.2\.0$'
./sentinel --config tests/fixtures/valid.yaml --check-config | grep -q 'configuration OK'
./sentinel --config tests/fixtures/valid.yaml --interval 3 --check-config | grep -q 'interval_seconds=3'
./sentinel --config sentinel.example.yaml --check-config >/dev/null
./sentinel --once --no-processes --no-network --no-files >/dev/null 2>/dev/null

if ./sentinel --config tests/fixtures/invalid.yaml --check-config >/dev/null 2>&1; then
    echo "invalid config unexpectedly accepted" >&2
    exit 1
fi

if ./sentinel --interval 0 --check-config >/dev/null 2>&1; then
    echo "invalid interval unexpectedly accepted" >&2
    exit 1
fi

if ./sentinel --definitely-unknown >/dev/null 2>&1; then
    echo "unknown option unexpectedly accepted" >&2
    exit 1
fi

echo "cli tests passed"
