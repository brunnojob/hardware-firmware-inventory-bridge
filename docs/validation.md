# Validation record

Native build and unit tests passed locally and in GitHub Actions with compiler warnings treated as errors. Reproduce with `make all test`.

POSIX serial integration tests passed locally and in GitHub Actions. Reproduce with `make integration`. The tests use a pseudo-terminal, not a physical sensor.

ESP32 firmware board compilation passed in GitHub Actions using Arduino CLI, the Espressif core and Adafruit DHT libraries. Physical acquisition and upload to the author's board have not been performed by these checks.

Verified GitHub Actions run: https://github.com/brunnojob/hardware-firmware-inventory-bridge/actions/runs/38196514701

Native command output: [native-validation.txt](native-validation.txt). The original videos demonstrate the reference projects and are not recordings of these tests.
