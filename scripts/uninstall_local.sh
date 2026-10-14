#!/usr/bin/env bash
set -euo pipefail
target="${HOME}/.local/bin/syspulse"
if [[ -f "$target" ]]; then rm -- "$target"; echo "Removed $target"; else echo "Not installed at $target"; fi
