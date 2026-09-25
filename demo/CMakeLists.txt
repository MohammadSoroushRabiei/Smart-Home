# دموی ارائه: همان منطق واقعی فریمور (app_state/ml_agent/virtual_devices/led/lock)
# روی PC با سخت‌افزار مجازی - شبیه lv_demo_music ولی مالِ خود پروژه
cmake_minimum_required(VERSION 3.16)
project(smartsim-demo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MAIN ${CMAKE_CURRENT_SOURCE_DIR}/../main)

find_package(PkgConfig REQUIRED)

# SDL2 - اول CMake config، بعد pkg-config
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    set(SDL2_LINK SDL2::SDL2)
else()
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    set(SDL2_LINK PkgConfig::SDL2)
endif()

include(FetchContent)

# git توزیع اوبونتو فقط backend گواهی gnutls دارد؛ اگر ~/.gitconfig
# http.sslBackend=openssl بگذارد، کلون‌های https می‌شکنند
set(ENV{GIT_CONFIG_GLOBAL} "/dev/null")

add_compile_definitions(LV_CONF_INCLUDE_SIMPLE)
# بعضی هدرهای برد bool را بدون stdbool.h استفاده می‌کنند (روی IDF ترانزیتور
# می‌آید) - اجباری سراسری می‌کنیم
add_compile_options(-include stdbool.h)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# همان نسخه‌ای که فریمور استفاده می‌کند
FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        v9.5.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(lvgl)

target_link_libraries(lvgl PUBLIC ${SDL2_LINK})

add_executable(smartdemo
    src/main.c
    src/shims.c
    src/scenario.c
    # ===== منطق واقعی فریمور - دست‌نخورده =====
    ${MAIN}/app_state.c
    ${MAIN}/ml_agent.c
    ${MAIN}/ml_model.c
    ${MAIN}/virtual_devices.c
    ${MAIN}/led.c
    ${MAIN}/lock.c
    # ===== UI واقعی - دست‌نخورده =====
    ${MAIN}/display/ui_screens.c
    ${MAIN}/display/keypad_screen.c
    ${MAIN}/display/setting_screen.c
    ${MAIN}/display/wifi_setup_screen.c
    ${MAIN}/display/mqtt_setup_screen.c
    ${MAIN}/display/attendance_screen.c)

target_include_directories(smartdemo PRIVATE
    src
    include
    ${MAIN}
    ${MAIN}/display)

target_link_libraries(smartdemo PRIVATE
    lvgl
    ${SDL2_LINK}
    pthread
    m)

target_compile_options(smartdemo PRIVATE -Wall -Wno-unused-parameter)
