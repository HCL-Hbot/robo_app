cmake_minimum_required(VERSION 3.11)

project(server_app)

include(FetchContent)

add_subdirectory(rtp_receiver)

set(WHISPER_CUDA 1)
set(LLAMA_CUDA 1)

FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp
    GIT_TAG v1.6.2
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/whisper
)
FetchContent_MakeAvailable(whisper)

# add_library(whisper_alt ${CMAKE_CURRENT_LIST_DIR}/whisper_alt/whisper.cpp)
# target_link_libraries(whisper_alt PUBLIC llama)
# target_include_directories(whisper_alt PUBLIC ${CMAKE_CURRENT_LIST_DIR}/whisper_alt)


find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_library(audio_buffer ${CMAKE_CURRENT_LIST_DIR}/audio_buffer/audio_buffer.cpp)
target_link_libraries(audio_buffer rtp_receiver)
target_include_directories(audio_buffer PUBLIC ${CMAKE_CURRENT_LIST_DIR}/audio_buffer/)

add_library(llama ${CMAKE_CURRENT_LIST_DIR}/llama/llama.cpp
                  ${CMAKE_CURRENT_LIST_DIR}/llama/unicode.cpp
                  ${CMAKE_CURRENT_LIST_DIR}/llama/unicode-data.cpp)
target_include_directories(llama PUBLIC ${CMAKE_CURRENT_LIST_DIR}/llama/)
target_link_libraries(llama whisper)



add_executable(main main.cpp common.cpp)
target_link_libraries(main PUBLIC rtp_receiver audio_buffer ${SDL2_LIBRARIES})
target_link_libraries(main PRIVATE llama)
target_link_libraries(main PRIVATE whisper)
target_include_directories(main PUBLIC ".")