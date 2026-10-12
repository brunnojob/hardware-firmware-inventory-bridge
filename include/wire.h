#ifndef TELEMETRY_WIRE_H
#define TELEMETRY_WIRE_H
#include <stddef.h>
#include <stdint.h>
#define WIRE_FRAME_SIZE 20
typedef struct { uint32_t sequence; uint32_t uptime; int16_t temperature; uint16_t humidity; uint8_t presence; uint8_t valid; } wire_sample;
static inline uint16_t wire_crc(const uint8_t* data, size_t length) {
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit) crc = (uint16_t)((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
    }
    return crc;
}
static inline void wire_put16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static inline uint16_t wire_get16(const uint8_t* p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static inline void wire_put32(uint8_t* p, uint32_t v) { for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
static inline uint32_t wire_get32(const uint8_t* p) { uint32_t v = 0; for (unsigned i = 0; i < 4; ++i) v |= (uint32_t)p[i] << (8 * i); return v; }
static inline void wire_encode(const wire_sample* sample, uint8_t* frame) {
    frame[0] = 0xa5; frame[1] = 0x5a; frame[2] = 1; frame[3] = 1;
    wire_put32(frame + 4, sample->sequence); wire_put32(frame + 8, sample->uptime);
    wire_put16(frame + 12, (uint16_t)sample->temperature); wire_put16(frame + 14, sample->humidity);
    frame[16] = sample->presence; frame[17] = sample->valid;
    wire_put16(frame + 18, wire_crc(frame, 18));
}
static inline int wire_decode(const uint8_t* frame, wire_sample* sample) {
    if (frame[0] != 0xa5 || frame[1] != 0x5a || frame[2] != 1 || frame[3] != 1 || wire_crc(frame, 18) != wire_get16(frame + 18)) return 0;
    sample->sequence = wire_get32(frame + 4); sample->uptime = wire_get32(frame + 8);
    uint16_t raw = wire_get16(frame + 12);
    sample->temperature = (int16_t)(raw < 0x8000 ? (int32_t)raw : (int32_t)raw - 65536);
    sample->humidity = wire_get16(frame + 14); sample->presence = frame[16]; sample->valid = frame[17];
    if (sample->valid > 1 || sample->presence > 1 || (sample->valid && (sample->humidity > 10000 || sample->temperature < -4000 || sample->temperature > 8000))) return 0;
    return 1;
}
typedef struct { uint8_t bytes[WIRE_FRAME_SIZE]; size_t count; } wire_parser;
static inline int wire_feed(wire_parser* parser, uint8_t byte, wire_sample* sample) {
    if (parser->count < WIRE_FRAME_SIZE) parser->bytes[parser->count++] = byte;
    if (parser->count < WIRE_FRAME_SIZE) return 0;
    if (wire_decode(parser->bytes, sample)) { parser->count = 0; return 1; }
    for (size_t i = 1; i < WIRE_FRAME_SIZE; ++i) parser->bytes[i - 1] = parser->bytes[i];
    parser->count = WIRE_FRAME_SIZE - 1;
    return 0;
}
#endif
