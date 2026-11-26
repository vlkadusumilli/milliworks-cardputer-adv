// app_sd.cpp - shows what microSD card is inserted and lets you browse its files.
// The card talks to the ESP32 over SPI (4 wires: clock, data in, data out, chip select).
// Everything shown is also printed to the USB serial port (pio device monitor).
#include <SPI.h>
#include <SD.h>
#include "ui.h"
#include "apps.h"
#include "config.h"

// microSD pins on the Cardputer ADV (from the M5Stack docs).
#define SD_SCK  40
#define SD_MISO 39
#define SD_MOSI 14
#define SD_CS   12

static bool mounted = false;

// Makes a folder and every folder above it ("/a/b/c" -> /a, /a/b, /a/b/c).
void sdMakeDirs(const char* path) {
    String p = path, part;
    int from = 1;
    while (from <= (int)p.length()) {
        int slash = p.indexOf('/', from);
        if (slash < 0) slash = p.length();
        part = p.substring(0, slash);
        if (!SD.exists(part)) SD.mkdir(part);
        from = slash + 1;
    }
}

static bool isDir(const String& path) {
    File f = SD.open(path);
    bool d = f && f.isDirectory();
    if (f) f.close();
    return d;
}

static void moveIfThere(const String& from, const String& to) {
    if (SD.exists(from) && !SD.exists(to) && SD.rename(from, to))
        Serial.printf("SD: moved %s -> %s\n", from.c_str(), to.c_str());
}

// Brings a card from an older version into the /milliWorks layout (see config.h).
static void tidyFolders() {
    // The card ignores upper/lower case, so an old "milliworks" folder IS /milliWorks;
    // rename it (through a temporary name) so it shows as milliWorks on a computer too.
    File root = SD.open("/");
    String oldName;
    if (root) {
        for (File f = root.openNextFile(); f; f = root.openNextFile()) {
            String n = f.name();
            if (n.startsWith("/")) n = n.substring(1);
            if (f.isDirectory() && n.equalsIgnoreCase("milliworks") && n != "milliWorks") oldName = n;
            f.close();
        }
        root.close();
    }
    if (oldName.length() && SD.rename("/" + oldName, "/mw-rename") && SD.rename("/mw-rename", MW_DIR))
        Serial.println("SD: renamed /" + oldName + " -> " MW_DIR);
    sdMakeDirs(SETTINGS_DIR);
    sdMakeDirs(BREATHE_DIR);
    sdMakeDirs(FLOW_DIR);
    if (isDir(MW_DIR "/music")) SD.rmdir(MW_DIR "/music");   // v16-v19 made it; only goes if empty
    moveIfThere(MW_DIR "/settings.json", SETTINGS_DIR "/settings.json");
    if (isDir(MW_DIR "/voice")) moveIfThere(MW_DIR "/voice", VOICE_DIR);
    sdMakeDirs(VOICE_DIR);
    moveIfThere("/flow/parked.txt", FLOW_DIR "/parked.txt");
    if (isDir("/flow")) SD.rmdir("/flow");             // only goes if it is now empty
}

bool sdMount() {
    if (mounted) return true;
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    mounted = SD.begin(SD_CS, SPI, 25000000);
    if (mounted) tidyFolders();
    return mounted;
}

static String humanSize(uint64_t bytes) {
    if (bytes >= 1024ULL * 1024 * 1024) return String(bytes / 1073741824.0, 1) + " GB";
    if (bytes >= 1024 * 1024) return String(bytes / 1048576.0, 1) + " MB";
    if (bytes >= 1024) return String(bytes / 1024.0, 1) + " KB";
    return String((uint32_t)bytes) + " B";
}

static const char* typeName(uint8_t t) {
    switch (t) {
        case CARD_MMC:  return "MMC";
        case CARD_SD:   return "SDSC";
        case CARD_SDHC: return "SDHC/SDXC";
        default:        return "Unknown";
    }
}

// Print the whole card tree to Serial (up to a few levels deep).
static void dumpTree(const String& path, int depth) {
    File dir = SD.open(path);
    if (!dir) return;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        String name = f.name();
        String full = path == "/" ? "/" + name : path + "/" + name;
        for (int i = 0; i < depth; i++) Serial.print("  ");
        if (f.isDirectory()) {
            Serial.printf("[%s]\n", name.c_str());
            if (depth < 3) dumpTree(full, depth + 1);
        } else {
            Serial.printf("%s  (%s)\n", name.c_str(), humanSize(f.size()).c_str());
        }
        f.close();
    }
    dir.close();
}

static void infoScreen() {
    uint8_t type = SD.cardType();
    uint64_t total = SD.cardSize();
    uint64_t fsTotal = SD.totalBytes();
    uint64_t used = SD.usedBytes();

    Serial.println("\n===== SD CARD =====");
    Serial.printf("Type: %s\nCard size: %s\nFilesystem size: %s\nUsed: %s\nFree: %s\n",
                  typeName(type), humanSize(total).c_str(), humanSize(fsTotal).c_str(),
                  humanSize(used).c_str(), humanSize(fsTotal - used).c_str());
    Serial.println("Files:");
    dumpTree("/", 1);
    Serial.println("===== END =====");

    drawHeader("SD Card");
    canvas.setFont(&fonts::Font2);
    canvas.setTextDatum(top_left);
    int y = 22;
    auto row = [&](const char* k, const String& v) {
        canvas.setTextColor(col::dim);
        canvas.drawString(k, 8, y);
        canvas.setTextColor(col::text);
        canvas.drawString(v, 90, y);
        y += 17;
    };
    row("Type", typeName(type));
    row("Card size", humanSize(total));
    row("Format", "FAT (readable)");
    row("Used", humanSize(used));
    row("Free", humanSize(fsTotal - used));
    // Usage bar.
    canvas.drawRoundRect(8, y + 2, SCREEN_W - 16, 8, 3, col::dim);
    if (fsTotal) canvas.fillRoundRect(9, y + 3, max<int>(2, (SCREEN_W - 18) * used / fsTotal), 6, 2, col::lilac);
    drawHints({{"OK", "browse files"}, {"ESC", "back"}});
    pushScreen();
}

struct Entry { String name; bool dir; uint64_t size; };

static std::vector<Entry> listDir(const String& path) {
    std::vector<Entry> out;
    File dir = SD.open(path);
    if (!dir) return out;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        out.push_back({f.name(), f.isDirectory(), f.size()});
        f.close();
    }
    dir.close();
    // Folders first, then alphabetical.
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
        if (a.dir != b.dir) return a.dir;
        return a.name.compareTo(b.name) < 0;
    });
    return out;
}

static void browse() {
    String path = "/";
    std::vector<Entry> entries = listDir(path);
    int selected = 0, top = 0;
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) {                                  // Back = up a folder, or leave at the top
            if (path == "/") return;
            path = path.substring(0, path.lastIndexOf('/'));
            if (path.isEmpty()) path = "/";
            entries = listDir(path);
            selected = top = 0;
            continue;
        }
        if (in.up && selected > 0) selected--;
        if (in.down && selected < (int)entries.size() - 1) selected++;
        if (in.enter && !entries.empty() && entries[selected].dir) {
            path = path == "/" ? "/" + entries[selected].name : path + "/" + entries[selected].name;
            entries = listDir(path);
            selected = top = 0;
        }

        std::vector<String> items;
        for (auto& e : entries)
            items.push_back(e.dir ? "[" + e.name + "]" : e.name + "  " + humanSize(e.size));
        if (items.empty()) items.push_back("(empty)");

        const int rows = 5;
        if (selected < top) top = selected;
        if (selected >= top + rows) top = selected - rows + 1;
        drawHeader(path.c_str());
        drawList(items, selected, top, rows);
        drawHints({{ARROWS_UD, "move"}, {"OK", "open folder"}, {"ESC", path == "/" ? "back" : "up"}});
        pushScreen();
        delay(15);
    }
}

void appSd() {
    if (!sdMount()) {
        Serial.println("SD: could not mount card (missing, or not FAT32)");
        drawHeader("SD Card");
        canvas.setFont(&fonts::Font2);
        canvas.setTextDatum(middle_center);
        canvas.setTextColor(col::warn);
        canvas.drawString("No readable card found", SCREEN_W / 2, 50);
        canvas.setTextColor(col::text);
        canvas.drawString("Check it's inserted and", SCREEN_W / 2, 75);
        canvas.drawString("formatted as FAT32", SCREEN_W / 2, 93);
        drawHints({{"ESC", "back"}});
        pushScreen();
        while (true) {
            M5Cardputer.update();
            if (readInput().back) return;
            delay(15);
        }
    }
    infoScreen();
    while (true) {
        M5Cardputer.update();
        Input in = readInput();
        if (in.back) return;
        if (in.enter) { browse(); infoScreen(); }
        delay(15);
    }
}
