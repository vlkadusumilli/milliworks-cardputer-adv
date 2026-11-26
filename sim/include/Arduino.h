// Arduino.h (simulator) - just enough of Arduino and ESP32/FreeRTOS for milliWorks to run
// on a Mac. Included first in every firmware file (see sim/Makefile).
#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <sys/time.h>
#include <time.h>

#ifndef PI
#define PI 3.14159265358979f
#endif

uint32_t millis();
inline void delay(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

template <class A, class B> inline auto min(A a, B b) -> typename std::common_type<A, B>::type { return b < a ? b : a; }
template <class A, class B> inline auto max(A a, B b) -> typename std::common_type<A, B>::type { return a < b ? b : a; }
template <class T, class L, class H> inline T constrain(T x, L lo, H hi) { return x < (T)lo ? (T)lo : (x > (T)hi ? (T)hi : x); }

// ---- String --------------------------------------------------------------------------
class String : public std::string {
public:
    String() {}
    String(const char* s) : std::string(s ? s : "") {}
    String(const std::string& s) : std::string(s) {}
    String(char c) : std::string(1, c) {}
    String(int v) : std::string(std::to_string(v)) {}
    String(unsigned v) : std::string(std::to_string(v)) {}
    String(long v) : std::string(std::to_string(v)) {}
    String(unsigned long v) : std::string(std::to_string(v)) {}
    String(long long v) : std::string(std::to_string(v)) {}
    String(unsigned long long v) : std::string(std::to_string(v)) {}
    String(double v, int decimals = 2) { char b[64]; snprintf(b, sizeof(b), "%.*f", decimals, v); assign(b); }
    // M5GFX's String overloads exist only in Arduino builds; this lets its const char* ones take a String.
    operator const char*() const { return c_str(); }
    unsigned length() const { return (unsigned)size(); }
    bool isEmpty() const { return empty(); }
    bool startsWith(const String& p) const { return compare(0, p.size(), p) == 0 && size() >= p.size(); }
    bool endsWith(const String& p) const { return size() >= p.size() && compare(size() - p.size(), p.size(), p) == 0; }
    String substring(unsigned from) const { return from >= size() ? String() : String(substr(from)); }
    String substring(unsigned from, unsigned to) const { if (to > size()) to = size(); return from >= to ? String() : String(substr(from, to - from)); }
    int indexOf(char c) const { auto p = find(c); return p == npos ? -1 : (int)p; }
    int indexOf(const String& s) const { auto p = find(s); return p == npos ? -1 : (int)p; }
    int indexOf(char c, unsigned from) const { auto p = find(c, from); return p == npos ? -1 : (int)p; }
    bool equalsIgnoreCase(const String& o) const {
        if (size() != o.size()) return false;
        for (size_t i = 0; i < size(); i++) if (tolower((unsigned char)(*this)[i]) != tolower((unsigned char)o[i])) return false;
        return true;
    }
    int lastIndexOf(char c) const { auto p = rfind(c); return p == npos ? -1 : (int)p; }
    long toInt() const { return strtol(c_str(), nullptr, 10); }
    float toFloat() const { return strtof(c_str(), nullptr); }
    void remove(unsigned index) { if (index < size()) erase(index); }
    void remove(unsigned index, unsigned count) { if (index < size()) erase(index, count); }
    void setCharAt(unsigned i, char c) { if (i < size()) (*this)[i] = c; }
    char charAt(unsigned i) const { return i < size() ? (*this)[i] : 0; }
    void toLowerCase() { for (auto& c : *this) c = tolower((unsigned char)c); }
    void toUpperCase() { for (auto& c : *this) c = toupper((unsigned char)c); }
    void trim() { size_t a = find_first_not_of(" \t\r\n"); size_t b = find_last_not_of(" \t\r\n"); *this = a == npos ? String() : String(substr(a, b - a + 1)); }
    int compareTo(const String& o) const { return compare(o); }
    bool equals(const String& o) const { return *this == o; }
    String& operator+=(const String& o) { append(o); return *this; }
    String& operator+=(const char* o) { append(o ? o : ""); return *this; }
    String& operator+=(char c) { push_back(c); return *this; }
    String& operator+=(int v) { append(std::to_string(v)); return *this; }
};
inline String operator+(const String& a, const String& b) { String r(a); r.append(b); return r; }
inline String operator+(const String& a, const char* b) { String r(a); r.append(b ? b : ""); return r; }
inline String operator+(const char* a, const String& b) { String r(a); r.append(b); return r; }
inline String operator+(const String& a, char b) { String r(a); r.push_back(b); return r; }
inline String operator+(const String& a, int b) { return a + String(b); }
inline String operator+(const String& a, unsigned b) { return a + String(b); }
inline String operator+(const String& a, long b) { return a + String(b); }
inline String operator+(const String& a, unsigned long b) { return a + String(b); }

// ---- Serial ----------------------------------------------------------------------------
struct SimSerial {
    void begin(int) {}
    template <class T> void print(const T& v) { printf("%s", String(v).c_str()); }
    template <class T> void println(const T& v) { printf("%s\n", String(v).c_str()); }
    void println() { printf("\n"); }
    int printf(const char* f, ...) { va_list a; va_start(a, f); int n = vprintf(f, a); va_end(a); return n; }
};
extern SimSerial Serial;

// ---- FreeRTOS (tasks and queues) on std::thread --------------------------------------
typedef void* TaskHandle_t;
typedef int BaseType_t;
typedef uint32_t TickType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
inline void vTaskDelay(TickType_t t) { delay(t); }
inline BaseType_t xTaskCreatePinnedToCore(void (*fn)(void*), const char*, uint32_t, void* arg, int, TaskHandle_t* h, int) {
    std::thread(fn, arg).detach();
    if (h) *h = (void*)1;
    return pdPASS;
}
struct SimQueue {
    std::mutex m; std::condition_variable cv; std::deque<std::vector<uint8_t>> items; size_t cap, size;
};
typedef SimQueue* QueueHandle_t;
inline QueueHandle_t xQueueCreate(size_t cap, size_t size) { auto q = new SimQueue(); q->cap = cap; q->size = size; return q; }
inline BaseType_t xQueueSend(QueueHandle_t q, const void* item, TickType_t) {
    std::lock_guard<std::mutex> l(q->m);
    if (q->items.size() >= q->cap) return pdFALSE;
    q->items.emplace_back((const uint8_t*)item, (const uint8_t*)item + q->size);
    q->cv.notify_one();
    return pdTRUE;
}
inline BaseType_t xQueueReceive(QueueHandle_t q, void* item, TickType_t wait) {
    std::unique_lock<std::mutex> l(q->m);
    if (!q->cv.wait_for(l, std::chrono::milliseconds(wait == portMAX_DELAY ? 1000000000u : wait), [&] { return !q->items.empty(); })) return pdFALSE;
    memcpy(item, q->items.front().data(), q->size);
    q->items.pop_front();
    return pdTRUE;
}
inline size_t uxQueueMessagesWaiting(QueueHandle_t q) { std::lock_guard<std::mutex> l(q->m); return q->items.size(); }

// ---- time: the simulator keeps its own clock so "set time by hand" doesn't touch the Mac ----
time_t sim_time(time_t* t);
int sim_settimeofday(const struct timeval* tv, const struct timezone* tz);
#define time(x) sim_time(x)
#define settimeofday(a, b) sim_settimeofday(a, b)
void configTzTime(const char* tz, const char* s1, const char* s2 = nullptr, const char* s3 = nullptr);
