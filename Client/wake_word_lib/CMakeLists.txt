set(COMMON_LIBS dl)
set(MIC_LIBS pthread m)

include(FetchContent)

FetchContent_Declare(
pvrecorder
GIT_REPOSITORY https://github.com/Picovoice/pvrecorder
GIT_TAG 7faea93bd80873083ce94f372a2af74c8269d611
SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/pvrecorder
)
FetchContent_MakeAvailable(pvrecorder)

add_library(wake_word_lib wake_word_lib_hooks.cpp)
target_include_directories(wake_word_lib PUBLIC pvrecorder/include "porcupine/inc" ".")

if (NOT WIN32)
    target_link_libraries(wake_word_lib ${COMMON_LIBS} ${MIC_LIBS} $<TARGET_OBJECTS:pv_recorder_object>)
    if((${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm") AND (UNIX AND NOT APPLE))
        target_link_libraries(wake_word_lib PUBLIC atomic)
    endif()
endif()
