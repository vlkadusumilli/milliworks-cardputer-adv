// test_sd_features.cpp - tests v14's SD-card features on a computer, using the SAME code
// the Cardputer runs (src/settings_json.h, src/wav_parse.h):
//
//   c++ -std=c++17 -O2 -I src tools/test_sd_features.cpp -o /tmp/test_sd && /tmp/test_sd
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
#include "settings_json.h"
#include "wav_parse.h"

static int passed = 0, failed = 0;
static void check(bool ok, const std::string& what) {
    printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    (ok ? passed : failed)++;
}

// Builds a settings file exactly like settings_sd.cpp's writeFile().
static std::string makeFile(const std::map<std::string, std::string>& text, const std::map<std::string, long>& nums,
                            const std::map<std::string, bool>& flags) {
    std::string body = "{\n  \"app\": \"" + jsonEscape("milliWorks") + "\"";
    body += jsonText("version", "14");
    body += jsonText("device", "Cardputer ADV");
    body += jsonText("saved", "2026-11-26 14:05");
    body += jsonBool("wifi_passwords", true);
    for (auto& kv : nums) body += jsonInt(kv.first, kv.second);
    for (auto& kv : flags) body += jsonBool(kv.first, kv.second);
    for (auto& kv : text) body += jsonText(kv.first, kv.second);
    return body + "\n}\n";
}

static std::vector<uint8_t> wav(uint16_t channels, uint16_t bits, uint32_t rate, uint32_t samples, bool listChunk) {
    std::vector<uint8_t> b;
    auto s = [&](const char* t) { b.insert(b.end(), t, t + 4); };
    auto u16 = [&](uint32_t v) { b.push_back(v & 255); b.push_back(v >> 8); };
    auto u32 = [&](uint32_t v) { u16(v & 0xFFFF); u16(v >> 16); };
    uint32_t data = samples * channels * bits / 8;
    s("RIFF"); u32(0); s("WAVE");
    if (listChunk) { s("LIST"); u32(27); for (int i = 0; i < 27; i++) b.push_back('x'); b.push_back(0); }   // odd size: padded
    s("fmt "); u32(16); u16(1); u16(channels); u32(rate); u32(rate * channels * bits / 8); u16(channels * bits / 8); u16(bits);
    s("data"); u32(data);
    for (uint32_t i = 0; i < data; i++) b.push_back(i & 255);
    return b;
}

int main() {
    printf("Settings file (settings_json.h):\n");
    {
        // Values like yours: a Wi-Fi password with quotes and +, a text with a line break, accents.
        std::map<std::string, std::string> text = {
            {"s0", "HomeWiFi"}, {"p0", "\"fake+pass=word\""}, {"s1", "Caf\xC3\xA9 Wi-Fi"}, {"p1", "back\\slash"},
            {"fl_int", "essay intro\nsecond line"}, {"voice", "samantha"}};
        std::map<std::string, long> nums = {{"theme", 0}, {"tz", 3}, {"bright", 180}, {"snd_vol", 190}, {"br_gd", 0}};
        std::map<std::string, bool> flags = {{"h24", false}, {"snd_on", true}, {"flip", false}, {"br_mus", true}};
        std::string file = makeFile(text, nums, flags);
        std::map<std::string, std::string> m;
        check(parseFlatJson(file, m), "a saved file reads back");
        bool same = true;
        for (auto& kv : text) same &= m.count(kv.first) && m[kv.first] == "\"" + kv.second;
        check(same, "every text comes back exactly (quotes, +, =, backslash, line break, accents)");
        bool numsOk = true;
        for (auto& kv : nums) numsOk &= m[kv.first] == std::to_string(kv.second);
        check(numsOk, "numbers come back exactly");
        check(m["h24"] == "false" && m["snd_on"] == "true", "true/false come back exactly");
        check(m["app"] == "\"milliWorks" && m["wifi_passwords"] == "true", "file header (app, passwords flag)");

        // A file someone edited on a Mac: different spacing, order and line endings.
        std::string edited = "{\r\n\"theme\":2,\"app\" : \"milliWorks\" ,\r\n  \"voice\":\"daniel\"}";
        check(parseFlatJson(edited, m) && m["theme"] == "2" && m["voice"] == "\"daniel", "a hand-edited file (spacing, order, CRLF) reads");
        check(parseFlatJson("{\"s0\": \"\\u00e9t\\u00e9\"}", m) && m["s0"] == "\"\xC3\xA9t\xC3\xA9", "\\u escapes become proper letters");

        // Damaged files must be refused, not half-loaded.
        check(!parseFlatJson(file.substr(0, file.size() / 2), m), "a half-written file is refused");
        check(!parseFlatJson("{\"theme\": 2, \"voice\": \"sam", m), "an unterminated text is refused");
        check(!parseFlatJson("{\"theme\": two}", m), "a word where a number should be is refused");
        check(!parseFlatJson("", m), "an empty file is refused");
        check(!parseFlatJson("hello", m), "a file that isn't settings is refused");
        check(parseFlatJson("{}", m) && m.empty(), "an empty settings object is fine (nothing changes)");
    }

    printf("\nVoice files (wav_parse.h):\n");
    {
        WavInfo w;
        const char* sd[] = {"design/sd/milliWorks/breathe/voices/luna/in.wav", "design/sd/milliWorks/breathe/voices/luna/hold.wav",
                            "design/sd/milliWorks/breathe/voices/luna/out.wav"};
        for (auto path : sd) {
            std::ifstream f(path, std::ios::binary);
            std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
            bool ok = parseWavHeader(b.data(), std::min<size_t>(b.size(), 4096), w);
            check(ok && w.channels == 1 && w.rate == 16000 && w.bits == 16 && w.dataOffset + w.dataBytes <= b.size(),
                  std::string("the SD voice file reads: ") + path);
        }
        auto st = wav(2, 16, 44100, 1000, true);
        check(parseWavHeader(st.data(), st.size(), w) && w.channels == 2 && w.rate == 44100 && w.dataOffset == 12 + 8 + 28 + 8 + 16 + 8,
              "stereo 44.1 kHz with a metadata chunk (e.g. from Audacity) reads, data found after it");
        auto r22 = wav(1, 16, 22050, 500, false);
        check(parseWavHeader(r22.data(), r22.size(), w) && w.rate == 22050, "other sample rates (22 kHz) read");
        auto b8 = wav(1, 8, 16000, 500, false);
        check(!parseWavHeader(b8.data(), b8.size(), w), "8-bit files are refused (not supported)");
        check(!parseWavHeader(st.data(), 30, w), "a cut-off file is refused");
        std::vector<uint8_t> mp3 = {'I', 'D', '3', 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        check(!parseWavHeader(mp3.data(), mp3.size(), w), "an MP3 renamed to .wav is refused");
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
