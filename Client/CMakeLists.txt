cmake_minimum_required(VERSION 3.6)
project(porcupine_demo_c)

include(FetchContent)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

FetchContent_Declare(
        openwakeword
        GIT_REPOSITORY https://github.com/Hoog-V/Openwakeword.Cpp.git
        GIT_TAG main
        SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/Openwakeword.cpp
)

FetchContent_MakeAvailable(openwakeword)
add_subdirectory(rtp_lib)
add_subdirectory(rtp_receiver)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

include_directories(${MOSQUITTO_INCLUDE_DIRS})
link_directories(${MOSQUITTO_LIBRARY_DIRS})

add_executable(
        porcupine_demo_mic
        main.cpp)

target_link_libraries(porcupine_demo_mic rtp_streamer rtp_receiver ${MOSQUITTO_LIBRARIES} openwakeword)
