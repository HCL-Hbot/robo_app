find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GST_RTP REQUIRED gstreamer-rtp-1.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)

add_library(rtp_streamer rtp_streamer.cpp)
target_include_directories(rtp_streamer PUBLIC "." ${GST_INCLUDE_DIRS} ${GST_APP_INCLUDE_DIRS} ${GST_RTP_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS})

target_link_libraries(rtp_streamer ${GST_LIBRARIES} ${GST_APP_LIBRARIES} ${GST_RTP_LIBRARIES})