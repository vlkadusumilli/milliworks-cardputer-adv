# Builds the milliWorks firmware (../src) for the Mac, drawing with the real M5GFX library
# into an SDL window. Needs: brew install sdl2, and `pio pkg install` once (for M5GFX).
#
#   make -C sim          build
#   make -C sim run      run (settings kept in sim/data, SD card = sim/sdcard)
#   make -C sim run-new  run like a brand-new Cardputer (welcome tour)
#   make -C sim run-keys type on the real Cardputer (Settings > Keyboard bridge on it)

M5GFX   := ../.pio/libdeps/cardputer-adv/M5GFX/src
SDLCF   := $(shell sdl2-config --cflags)
SDLLIB  := $(shell sdl2-config --libs)
DEFS    := -DLGFX_SDL -DM5GFX_BOARD=board_M5CardputerADV -DM5GFX_SCALE=3
CXXSTD  := -std=c++17 -O1 -g0 -w
OUT     := build

LIB_CPP := $(shell find $(M5GFX) -name '*.cpp')
LIB_C   := $(shell find $(M5GFX) -name '*.c')
FW_CPP  := $(filter-out ../src/wifitest/%,$(wildcard ../src/*.cpp))
SIM_CPP := sim_platform.cpp sim_main.cpp

LIB_OBJ := $(patsubst $(M5GFX)/%,$(OUT)/lib/%.o,$(LIB_CPP) $(LIB_C))
FW_OBJ  := $(patsubst ../src/%.cpp,$(OUT)/fw/%.o,$(FW_CPP))
SIM_OBJ := $(patsubst %.cpp,$(OUT)/sim/%.o,$(SIM_CPP))

milliworks-sim: $(LIB_OBJ) $(FW_OBJ) $(SIM_OBJ)
	$(CXX) $^ $(SDLLIB) -o $@
	@echo "Built sim/milliworks-sim"

# The screen library, unchanged.
$(OUT)/lib/%.cpp.o: $(M5GFX)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(DEFS) $(SDLCF) -I/opt/homebrew/include -I$(M5GFX) -c $< -o $@
$(OUT)/lib/%.c.o: $(M5GFX)/%.c
	@mkdir -p $(dir $@)
	$(CC) -O1 -w -c $< -o $@

# The firmware: Arduino/ESP32 headers come from sim/include.
$(OUT)/fw/%.o: ../src/%.cpp ../src/*.h include/*.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(DEFS) $(SDLCF) -I/opt/homebrew/include -Iinclude -I../src -I$(M5GFX) -include Arduino.h -c $< -o $@

$(OUT)/sim/%.o: %.cpp include/*.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) $(DEFS) $(SDLCF) -I/opt/homebrew/include -Iinclude -I../src -I$(M5GFX) -include Arduino.h -c $< -o $@

# The SD card starts with the voices you'd copy to the real card (milliWorks/breathe/voices).
VOICE := milliWorks/breathe/voices
sdcard/$(VOICE)/luna/in.wav: ../design/sd/$(VOICE)/luna/in.wav
	@mkdir -p sdcard/$(VOICE)
	cp -R ../design/sd/$(VOICE)/luna ../design/sd/$(VOICE)/julian sdcard/$(VOICE)/

run: milliworks-sim sdcard/$(VOICE)/luna/in.wav
	./milliworks-sim

run-new: milliworks-sim sdcard/$(VOICE)/luna/in.wav
	./milliworks-sim --fresh

# Type on the real Cardputer: start Settings > Keyboard bridge on it first.
run-keys: milliworks-sim sdcard/$(VOICE)/luna/in.wav
	./milliworks-sim --cardputer-keys

clean:
	rm -rf $(OUT) milliworks-sim

.PHONY: run run-new run-keys clean
