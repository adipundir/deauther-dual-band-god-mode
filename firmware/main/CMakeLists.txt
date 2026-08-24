idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event nvs_flash esp_netif console driver
             wifi deauth godmode control http_server led)

# Bypass Espressif's raw-frame sanity check so deauth/disassoc frames transmit.
# The __wrap_ implementation lives in deauth_engine.c. This must land on the
# FINAL executable link, so set it as a global build link option.
idf_build_set_property(LINK_OPTIONS "-Wl,-wrap=ieee80211_raw_frame_sanity_check" APPEND)
