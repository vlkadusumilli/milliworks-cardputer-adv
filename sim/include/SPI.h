// SPI.h (simulator): the SD card is a folder, no SPI needed.
#pragma once
#include "Arduino.h"
struct SimSPI { void begin(int = 0, int = 0, int = 0, int = 0) {} };
extern SimSPI SPI;
