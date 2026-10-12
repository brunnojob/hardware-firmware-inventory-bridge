# Hardware contract

The video establishes an ESP32-class sensor project. Exact board revision and pin wiring cannot be recovered reliably from the recording. These are explicit defaults for this implementation.

| Component | Signal | Default ESP32 connection |
| --- | --- | --- |
| DHT11 | Data | GPIO 4 |
| DHT11 | Supply | 3.3 V |
| DHT11 | Ground | GND |
| PIR module | Digital output | GPIO 27 |
| PIR module | Ground | GND |
| ESP32 | Serial transport | USB to host |

For a bare DHT sensor, use a data pull-up to 3.3 V according to its module requirements. Configure PIR supply according to the actual module; verify its output is compatible with a 3.3 V GPIO. All grounds must be common. Do not connect a 5 V logic output directly to an ESP32 GPIO.

`DHT_PIN`, `DHT_TYPE` and `PIR_PIN` are compile-time settings. DHT11 is the default. For DHT22 use `DHT_TYPE=DHT22` and match wiring to its datasheet. The sketch samples once every 2000 ms, validates finite values and avoids presenting failed reads as real zero readings.

The inventory node additionally drives `STATUS_LED_PIN`, default GPIO 2, from debounced presence. Change it if your board uses a different LED or active-low wiring. No product scanner, load cell, lock actuator or automatic stock counter is implemented.
