find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GST_RTP REQUIRED gstreamer-rtp-1.0)

add_library(rtp_receiver rtp_receiver.cpp)
target_include_directories(rtp_receiver PUBLIC "." ${GST_INCLUDE_DIRS} ${GST_APP_INCLUDE_DIRS} ${GST_RTP_INCLUDE_DIRS})

target_link_libraries(rtp_receiver ${GST_LIBRARIES} ${GST_APP_LIBRARIES} ${GST_RTP_LIBRARIES})