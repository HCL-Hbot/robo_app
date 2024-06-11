#ifndef RTP_RECEIVER_HPP
#define RTP_RECEIVER_HPP
#include <gst/gst.h>
#include <functional>

/**
 * @class RtpReceiver
 * @brief A class to handle receiving RTP streams.
 * 
 * This class sets up a GStreamer pipeline to receive RTP streams on a specified port.
 */
class RtpReceiver {
public:
    /**
     * @brief Type definition for the sample callback function.
     * 
     * This callback is called when a new sample is received.
     * 
     * @param data Pointer to the received data.
     * @param len Length of the received data.
     */
    using SampleCallback = std::function<void(guint8* data, int len)>;

    /**
     * @brief Construct a new RtpReceiver object.
     * 
     * @param port The port to listen for RTP streams on.
     */
    RtpReceiver(int port);

    /**
     * @brief Destroy the RtpReceiver object.
     */
    ~RtpReceiver();

    bool isReceivingSamples();

    /**
     * @brief Pause receiving RTP streams.
     * 
     * This method pauses the GStreamer pipeline.
     */
    void pause();

    /**
     * @brief Resume receiving RTP streams.
     * 
     * This method resumes the GStreamer pipeline if it was previously paused.
     */
    void resume();

private:
    GstElement *pipeline;      ///< GStreamer pipeline element
    GstElement *udpsrc;        ///< GStreamer UDP source element
    GstElement *rtpdepay;      ///< GStreamer RTP depayload element
    GstElement *decoder;       ///< GStreamer decoder element
    GstElement *convert;       ///< GStreamer convert element
    GstElement *resample;      ///< GStreamer resample element
    GstElement *capsfilter;    ///< GStreamer caps filter element
    GstElement *sink;          ///< GStreamer sink element
    GstBus *bus;               ///< GStreamer bus
    GMainLoop *loop;           ///< GLib main loop
    SampleCallback sample_callback; ///< Callback function for received samples
};



#endif