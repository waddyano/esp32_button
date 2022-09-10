set(srcs "button_main.cpp"
         "esp_hid_gap.cpp" "mqtt.cpp" "ota.cpp" "wifi.cpp")
set(include_dirs ".")

idf_component_register(SRCS "index.cpp" "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES esp_hid esp_http_server esp_https_ota app_update json mqtt)
