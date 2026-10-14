#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ! -x build/syspulse_tests ]]; then ./scripts/build.sh; fi
ctest --test-dir build --output-on-failure
