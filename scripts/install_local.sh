#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ -x build/syspulse ]] || ./scripts/build.sh
dest="${HOME}/.local/bin"
mkdir -p "$dest"
if [[ -e "$dest/syspulse" ]]; then echo "Refusing to overwrite $dest/syspulse; move it first." >&2; exit 1; fi
install -m 0755 build/syspulse "$dest/syspulse"
echo "Installed to $dest/syspulse"
