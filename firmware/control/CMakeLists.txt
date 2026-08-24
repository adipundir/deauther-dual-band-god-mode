idf_component_register(
    SRCS "control.c"
    INCLUDE_DIRS "."
    REQUIRES wifi deauth godmode esp_timer)
