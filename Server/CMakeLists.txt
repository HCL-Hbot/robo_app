cmake_minimum_required(VERSION 3.11)

project(server_app)

include(FetchContent)

add_subdirectory(src/rtp_receiver)

set(WHISPER_CUDA ON)

set(LLAMA_CUDA ON)

FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp
    GIT_TAG v1.6.2
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/lib/whisper
)
FetchContent_MakeAvailable(whisper)

add_definitions("-DGGML_USE_CUDA")

find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

include_directories(${MOSQUITTO_INCLUDE_DIRS})
link_directories(${MOSQUITTO_LIBRARY_DIRS})


find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_library(audio_buffer ${CMAKE_CURRENT_LIST_DIR}/src/audio_buffer/audio_buffer.cpp)
target_link_libraries(audio_buffer rtp_receiver)
target_include_directories(audio_buffer PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src/audio_buffer/)

add_library(llama ${CMAKE_CURRENT_LIST_DIR}/lib/llama/llama.cpp
                  ${CMAKE_CURRENT_LIST_DIR}/lib/llama/unicode.cpp
                  ${CMAKE_CURRENT_LIST_DIR}/lib/llama/unicode-data.cpp)
target_include_directories(llama PUBLIC ${CMAKE_CURRENT_LIST_DIR}/lib/llama/)
target_link_libraries(llama whisper)

add_library(common ${CMAKE_CURRENT_LIST_DIR}/src/common/common.cpp)
target_include_directories(common PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src/common)

add_library(whisper_wrapper ${CMAKE_CURRENT_LIST_DIR}/src/whisper_wrapper/whisper_wrapper.cpp)
target_include_directories(whisper_wrapper PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src/whisper_wrapper)
target_link_libraries(whisper_wrapper whisper common)

add_library(llama_wrapper ${CMAKE_CURRENT_LIST_DIR}/src/llama_wrapper/llama_wrapper.cpp)
target_include_directories(llama_wrapper PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src/llama_wrapper)
target_link_libraries(llama_wrapper whisper whisper_wrapper llama common)


add_executable(main src/main.cpp)
target_link_libraries(main PUBLIC rtp_receiver audio_buffer ${SDL2_LIBRARIES} ${MOSQUITTO_LIBRARIES} whisper_wrapper llama_wrapper common)
target_link_libraries(main PRIVATE llama)
target_link_libraries(main PRIVATE whisper)
target_include_directories(main PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)