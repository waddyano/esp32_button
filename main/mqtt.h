#pragma once

#include <functional>

extern void mqtt_start();
extern void mqtt_subscribe();
extern void mqtt_publish();
extern void mqtt_set_device_name(const char *name);
extern std::function<void(bool)> mqtt_on_state_published;
