# Serial protocol

UART over USB, 115200 baud, 8 data bits, no parity, 1 stop bit. All multibyte values are little-endian. Frame length is 20 bytes.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 2 | Magic A5 5A |
| 2 | 1 | Version 1 |
| 3 | 1 | Sample type 1 |
| 4 | 4 | Unsigned sequence |
| 8 | 4 | Unsigned uptime milliseconds |
| 12 | 2 | Signed temperature in 0.01 °C |
| 14 | 2 | Unsigned relative humidity in 0.01 percent |
| 16 | 1 | Presence, 0 or 1 |
| 17 | 1 | Valid sensor sample, 0 or 1 |
| 18 | 2 | CRC-16/CCITT-FALSE over bytes 0–17 |

CRC polynomial 0x1021, initial value 0xFFFF, no reflection and no final XOR. The checksum is encoded little-endian. Negative temperature is two's complement. Invalid samples carry zero temperature/humidity and a zero validity flag; presence remains independent.

The parser slides one byte at a time after invalid frames. The protocol header in the sketch directory is byte-identical to the native header. Boot detection is a heuristic based on early sequence and uptime; a future network protocol should include an explicit boot identifier. CRC detects corruption and provides no sender authentication.
