// esp_sntp.h (simulator): internet time "arrives" a moment after Wi-Fi connects.
#pragma once
#include "Arduino.h"
typedef void (*sntp_sync_time_cb_t)(struct timeval*);
void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t cb);
