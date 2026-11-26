// settings_json.h - writing and reading /milliWorks/settings/settings.json as plain C++ (no Arduino),
// so the same code runs on the Cardputer (settings_sd.cpp) and in tools/test_sd_features.cpp.
//
// The file is one flat JSON object: {"key": "text" | 123 | true | false, ...}
// In the map we read into, text values start with a '"' marker so they can't be confused
// with numbers or true/false ("\"hello" is the text hello; "12" is the number 12).
#pragma once
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

inline std::string jsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c == '\n') o += "\\n";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
        else o += (char)c;                                 // UTF-8 (accents, emoji) passes through
    }
    return o;
}

// One `"key": value` line, with the comma that separates it from the previous one.
inline std::string jsonText(const std::string& k, const std::string& v) { return ",\n  \"" + jsonEscape(k) + "\": \"" + jsonEscape(v) + "\""; }
inline std::string jsonInt(const std::string& k, long v) { return ",\n  \"" + jsonEscape(k) + "\": " + std::to_string(v); }
inline std::string jsonBool(const std::string& k, bool v) { return ",\n  \"" + jsonEscape(k) + "\": " + (v ? "true" : "false"); }

// Reads the flat object. Returns false for anything that isn't a complete, well-formed
// object (a damaged or half-written file), and then `out` must not be used.
inline bool parseFlatJson(const std::string& s, std::map<std::string, std::string>& out) {
    out.clear();
    size_t i = 0, n = s.size();
    auto skip = [&]() { while (i < n && isspace((unsigned char)s[i])) i++; };
    auto str = [&](std::string& v) -> bool {
        if (i >= n || s[i] != '"') return false;
        i++;
        v.clear();
        while (i < n && s[i] != '"') {
            if (s[i] == '\\') {
                if (++i >= n) return false;
                char e = s[i];
                if (e == 'n') v += '\n';
                else if (e == 't') v += '\t';
                else if (e == 'u') {
                    if (i + 4 >= n) return false;
                    long code = strtol(s.substr(i + 1, 4).c_str(), nullptr, 16);
                    if (code < 0x80) v += (char)code;
                    else if (code < 0x800) { v += (char)(0xC0 | (code >> 6)); v += (char)(0x80 | (code & 0x3F)); }
                    else { v += (char)(0xE0 | (code >> 12)); v += (char)(0x80 | ((code >> 6) & 0x3F)); v += (char)(0x80 | (code & 0x3F)); }
                    i += 4;
                } else v += e;                             // \" \\ \/
            } else v += s[i];
            i++;
        }
        if (i >= n) return false;                          // the text never ended: damaged file
        i++;
        return true;
    };
    skip();
    if (i >= n || s[i] != '{') return false;
    i++;
    while (true) {
        skip();
        if (i >= n) return false;                          // the object never closed
        if (s[i] == '}') { i++; break; }
        std::string key, val;
        if (!str(key)) return false;
        skip();
        if (i >= n || s[i] != ':') return false;
        i++;
        skip();
        if (i < n && s[i] == '"') { if (!str(val)) return false; val = "\"" + val; }
        else {
            size_t j = i;
            while (j < n && s[j] != ',' && s[j] != '}' && !isspace((unsigned char)s[j])) j++;
            val = s.substr(i, j - i);
            if (val.empty()) return false;
            if (!(val == "true" || val == "false")) {
                char* end = nullptr;
                strtol(val.c_str(), &end, 10);
                if (!end || *end) return false;            // not a number: damaged or edited wrong
            }
            i = j;
        }
        out[key] = val;
        skip();
        if (i < n && s[i] == ',') i++;
        else { skip(); if (i >= n || s[i] != '}') return false; }
    }
    return true;
}
