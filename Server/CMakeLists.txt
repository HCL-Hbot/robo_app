cmake_minimum_required(VERSION 3.11)

project(server_app)

include(FetchContent)

add_subdirectory(rtp_receiver)

set(WHISPER_CUDA 1)

FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp
    GIT_TAG v1.6.1
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/whisper
)

FetchContent_MakeAvailable(whisper)


find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_library(audio_buffer ${CMAKE_CURRENT_LIST_DIR}/audio_buffer/audio_buffer.cpp)
target_link_libraries(audio_buffer rtp_receiver)
target_include_directories(audio_buffer PUBLIC ${CMAKE_CURRENT_LIST_DIR}/audio_buffer/)

add_executable(main main.cpp common.cpp)
target_link_libraries(main rtp_receiver whisper  audio_buffer ${SDL2_LIBRARIES})
