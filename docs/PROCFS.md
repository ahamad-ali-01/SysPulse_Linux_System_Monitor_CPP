# Procfs parsing strategy

Process enumeration considers numeric directories under `/proc`. Each process is read independently; inaccessible or disappearing records are skipped. `/proc/[pid]/stat` is parsed by locating the final `)` around the command name before tokenizing the remaining fields. RSS is pages × `_SC_PAGESIZE`; virtual size is bytes. `/proc/[pid]/status` supplies UID. Command-line NUL separators are converted to spaces.

Procfs is a live view, not an atomic database. A PID can exit or be reused between reads. Permission settings can hide fields. SysPulse does not elevate privileges to work around restrictions.
