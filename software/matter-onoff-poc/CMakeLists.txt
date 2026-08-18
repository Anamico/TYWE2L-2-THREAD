# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly.
#
# This file is deliberately a near-verbatim copy of espressif/esp-matter's
# examples/light/CMakeLists.txt (Apache-2.0), trimmed of the encrypted-OTA
# and ESP32-C2 relinker special cases this project does not use. Reusing the
# real, working boilerplate is safer than inventing an equivalent, since the
# IDF_TARGET -> ESP_MATTER_DEVICE_PATH table and the EXTRA_COMPONENT_DIRS
# list encode build-system wiring (connectedhomeip's ESP32 component config,
# esp-matter's own components, the per-board device HAL) that is easy to get
# subtly wrong by hand.
cmake_minimum_required(VERSION 3.5)

if(NOT DEFINED ENV{ESP_MATTER_PATH})
    message(FATAL_ERROR "Please set ESP_MATTER_PATH to the path of your esp-matter checkout (source its export.sh first)")
endif(NOT DEFINED ENV{ESP_MATTER_PATH})

if(NOT DEFINED ENV{ESP_MATTER_DEVICE_PATH})
    if("${IDF_TARGET}" STREQUAL "esp32h2")
        set(ENV{ESP_MATTER_DEVICE_PATH} $ENV{ESP_MATTER_PATH}/device_hal/device/esp32h2_devkit_c)
    else()
        message(FATAL_ERROR "This project only targets esp32h2. Run 'idf.py set-target esp32h2' first.")
    endif()
endif(NOT DEFINED ENV{ESP_MATTER_DEVICE_PATH})

# esp32h2_devkit_c is Espressif's own reference board, not the DevKitM-1 this
# project actually targets. It is selected here only because the top-level
# build machinery requires *some* valid ESP_MATTER_DEVICE_PATH to exist for
# the target; this project's own main/ code does not call into it (no
# <device.h>, no led_driver_get_config()/button_driver_get_config()). The LED
# GPIO used below is taken directly from this repo's own schematic-verified
# docs/devkit-bringup.md, not from that board's defaults. See main/app_driver.cpp.

set(ESP_MATTER_PATH $ENV{ESP_MATTER_PATH})
set(MATTER_SDK_PATH ${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip)

# This should be done before using the IDF_TARGET variable.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include(${ESP_MATTER_PATH}/examples/common/cmake_common/components_include.cmake)
include($ENV{ESP_MATTER_DEVICE_PATH}/esp_matter_device.cmake)

set(EXTRA_COMPONENT_DIRS
    "${ESP_MATTER_PATH}/examples/common"
    "${MATTER_SDK_PATH}/config/esp32/components"
    "${ESP_MATTER_PATH}/components"
    "${ESP_MATTER_PATH}/device_hal/device"
    ${extra_components_dirs_append})

project(matter_onoff_poc)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H;-Wno-overloaded-virtual" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security" APPEND)
