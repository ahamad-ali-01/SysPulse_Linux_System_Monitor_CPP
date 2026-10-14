# Testing

Run `./scripts/build.sh` followed by `./scripts/run_tests.sh`. CTest executes the C++ test binary and CLI help smoke test. Optional sanitizer builds use `-DSYSPULSE_SANITIZERS=ON`. Tests avoid root and avoid asserting exact live system values.
