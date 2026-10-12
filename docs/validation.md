# Validation record

Native build and unit tests passed with GCC/G++ and warnings treated as errors. Reproduce with `make all test`.

The POSIX serial integration test passed. Reproduce with `make integration`. Test input uses a pseudo-terminal, not a physical sensor.

Arduino CLI and an ESP32 board are not available in the authoring environment. Firmware board compilation and physical acquisition have not been performed here. `make firmware` and the firmware CI job are configured for those checks. CI jobs have not run on GitHub because the repository has not been published.

Native command output: [native-validation.txt](native-validation.txt). Original videos are reference demonstrations rather than recordings of these tests.
