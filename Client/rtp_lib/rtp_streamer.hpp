#ifndef RTP_STREAMER_H
#define RTP_STREAMER_H
#include <gst/gst.h>
#include <gst/app/app.h>
#include <cstdint>

class RtpStreamer
{
public:
    RtpStreamer(const char *ip_addr, int port, uint32_t sampling_window, uint32_t sampling_rate);
    void start();
    void stop();
private:
    GstElement *pipeline, *audio_src, *convert, *resample, *encoder, *pay, *sink, *queue;
    uint32_t _sampling_window;
    uint32_t _sampling_rate;
    const char *ip_addr;
    int port;
    GstClock *clock;
};

#endif