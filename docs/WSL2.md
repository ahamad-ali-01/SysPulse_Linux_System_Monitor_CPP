# WSL2

Install from elevated PowerShell with `wsl --install -d Ubuntu`, launch Ubuntu, install `build-essential cmake unzip`, and build from the Linux filesystem. The monitor sees WSL's `/proc`, not all Windows processes. Use `\\wsl.localhost\Ubuntu\home\<user>` to browse Linux files from Windows.
