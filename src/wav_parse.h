// wav_parse.h - reads the header of a WAV file as plain C++ (no Arduino), shared by the
// firmware (voice.cpp) and tools/test_sd_features.cpp.
#pragma once
#include <cstdint>
#include <cstring>
#include <cstddef>

struct WavInfo {
    uint16_t channels = 0, bits = 0;
    uint32_t rate = 0;
    uint32_t dataOffset = 0, dataBytes = 0;   // where the samples start, and how many bytes
};

// `buf` holds the start of the file (4 KB is plenty, even with metadata chunks).
// Accepts plain PCM, 16-bit, mono or stereo, any sample rate. Everything else returns false.
inline bool parseWavHeader(const uint8_t* buf, size_t n, WavInfo& w) {
    auto le16 = [&](size_t p) { return (uint32_t)buf[p] | (uint32_t)buf[p + 1] << 8; };
    auto le32 = [&](size_t p) { return le16(p) | le16(p + 2) << 16; };
    w = WavInfo();
    if (n < 12 || memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) return false;
    size_t p = 12;
    bool haveFmt = false;
    while (p + 8 <= n) {
        uint32_t size = le32(p + 4);
        if (!memcmp(buf + p, "fmt ", 4)) {
            if (p + 8 + 16 > n || size < 16) return false;
            uint16_t format = le16(p + 8);
            w.channels = le16(p + 10);
            w.rate = le32(p + 12);
            w.bits = le16(p + 22);
            if (format != 1 || w.bits != 16 || w.channels < 1 || w.channels > 2 || w.rate < 4000 || w.rate > 48000) return false;
            haveFmt = true;
        } else if (!memcmp(buf + p, "data", 4)) {
            if (!haveFmt) return false;
            w.dataOffset = p + 8;
            w.dataBytes = size;
            return size >= 2u * w.channels;
        }
        p += 8 + size + (size & 1);                   // chunks are padded to an even size
    }
    return false;                                     // no sample data found in the header
}
