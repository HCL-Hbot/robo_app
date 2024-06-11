cmake_minimum_required(VERSION 3.6)
project(porcupine_demo_c)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

# add_subdirectory(wake_word_lib)
add_subdirectory(rtp_lib)
add_subdirectory(rtp_receiver)
add_subdirectory(Openwakeword.Cpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

include_directories(${MOSQUITTO_INCLUDE_DIRS})
link_directories(${MOSQUITTO_LIBRARY_DIRS})

add_executable(
        porcupine_demo_mic
        main.cpp)

target_link_libraries(porcupine_demo_mic rtp_streamer ${MOSQUITTO_LIBRARIES} openwakeword)
