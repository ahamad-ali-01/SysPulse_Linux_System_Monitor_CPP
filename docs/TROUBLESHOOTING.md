# Troubleshooting

- Install `build-essential` and `cmake` if the compiler or CMake is missing.
- CPU is 0 on first sample by design; `watch` provides repeated sampling.
- If process details are missing, inspect `/proc` visibility and user permissions.
- Export parent directories must exist and be writable.
- WSL2 metrics describe its Linux VM, not the entire Windows host.
