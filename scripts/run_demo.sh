#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ -x build/syspulse ]] || ./scripts/build.sh
./build/syspulse summary
./build/syspulse memory
./build/syspulse processes --sort memory --limit 10
