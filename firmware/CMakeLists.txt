# ESP32-C5 Dual-Band Deauther — ESP-IDF project
cmake_minimum_required(VERSION 3.16)

# The core modules live in sibling dirs; register them as components.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/wifi"
    "${CMAKE_CURRENT_LIST_DIR}/deauth"
    "${CMAKE_CURRENT_LIST_DIR}/godmode"
    "${CMAKE_CURRENT_LIST_DIR}/control"
    "${CMAKE_CURRENT_LIST_DIR}/http_server"
    "${CMAKE_CURRENT_LIST_DIR}/led")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(deauther_dual_band_god_mode)
