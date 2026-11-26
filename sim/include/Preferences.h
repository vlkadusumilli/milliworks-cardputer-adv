// Preferences.h (simulator) - the Cardputer's internal settings memory, kept in a file:
// sim/data/prefs.txt (one "key type value" per line). Delete it to start as a new device.
#pragma once
#include "Arduino.h"
#include <map>

class Preferences {
public:
    bool begin(const char* ns, bool readOnly = false);
    void end();
    bool isKey(const char* k);
    bool remove(const char* k);
    bool clear();

    int32_t getInt(const char* k, int32_t d = 0);
    bool getBool(const char* k, bool d = false);
    String getString(const char* k, const String& d = String());
    uint64_t getULong64(const char* k, uint64_t d = 0);
    size_t getBytes(const char* k, void* buf, size_t len);

    size_t putInt(const char* k, int32_t v);
    size_t putBool(const char* k, bool v);
    size_t putString(const char* k, const String& v);
    size_t putULong64(const char* k, uint64_t v);
    size_t putBytes(const char* k, const void* buf, size_t len);

private:
    std::string _ns;
    bool _ro = true;
    std::string key(const char* k) { return _ns + "." + k; }
    void set(const char* k, const std::string& typedValue);
    bool get(const char* k, std::string& typedValue);
};
