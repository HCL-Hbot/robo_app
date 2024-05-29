cmake_minimum_required(VERSION 3.6)
project(porcupine_demo_c)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(wake_word_lib)
add_subdirectory(rtp_lib)
add_subdirectory(com_cli_lib)

add_executable(
        porcupine_demo_mic
        main.cpp)

target_link_libraries(
        porcupine_demo_mic 
        wake_word_lib 
        rtp_streamer
        CLI_HOST
)
