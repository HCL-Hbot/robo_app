#ifndef RTP_RECEIVER_HPP
#define RTP_RECEIVER_HPP
#include <gst/gst.h>
#include <functional>

class RtpReceiver {
public:
    using SampleCallback = std::function<void(const float* data, size_t size)>;
    RtpReceiver(int port);
    ~RtpReceiver();
  void start(SampleCallback callback);
    void stop();

private:
    GstElement *pipeline, *udpsrc, *rtpdepay, *decoder, *convert, *resample, *capsfilter, *sink;
    GstBus *bus;
    GMainLoop *loop;
    SampleCallback sample_callback;
};


#endif