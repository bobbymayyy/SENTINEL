#!/bin/sh
set -eu

./sentinel --version | grep -q '^SENTINEL 0\.2\.0$'
./sentinel --config tests/fixtures/valid.yaml --check-config | grep -q 'configuration OK'
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

echo "cli tests passed"
