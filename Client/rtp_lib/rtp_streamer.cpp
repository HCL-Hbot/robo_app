#include <rtp_streamer.hpp>
#include <gst/gst.h>
#include <gst/sdp/gstsdpmessage.h>

static gboolean bus_callback(GstBus *bus, GstMessage *message, gpointer data) {
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error(message, &err, &debug_info);
            g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(message->src), err->message);
            g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free(debug_info);
            break;
        case GST_MESSAGE_STATE_CHANGED:
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);
            g_print("Element %s changed state from %s to %s.\n", GST_OBJECT_NAME(message->src),
                    gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
            break;
        default:
            break;
    }
    return TRUE;
}


RtpStreamer::RtpStreamer(const char *ip_addr, int port, uint32_t sampling_window, uint32_t sampling_rate)
    : _sampling_window(sampling_window), _sampling_rate(sampling_rate), ip_addr(ip_addr), port(port), clock(nullptr)
{
    gst_init(NULL, NULL);
    GstStateChangeReturn ret;

    // Create GStreamer elements
    pipeline = gst_pipeline_new("audio-sender");
    audio_src = gst_element_factory_make("autoaudiosrc", "audio-source");
    convert = gst_element_factory_make("audioconvert", "converter");
    resample = gst_element_factory_make("audioresample", "resampler");
    encoder = gst_element_factory_make("opusenc", "encoder");
    pay = gst_element_factory_make("rtpopuspay", "rtp-payload");
    sink = gst_element_factory_make("udpsink", "udp-sink");
    queue = gst_element_factory_make("queue", "queue");

    if (!pipeline || !audio_src || !convert || !resample || !encoder || !pay || !sink || !queue) {
        g_printerr("Failed to create GStreamer elements\n");
        exit(1);
    }

    // Set properties for low latency
    g_object_set(G_OBJECT(sink), "host", ip_addr, "port", port, NULL);
    g_object_set(G_OBJECT(queue), "max-size-time", 0, "max-size-buffers", 0, "max-size-bytes", 0, NULL);
    g_object_set(G_OBJECT(encoder), "bitrate", sampling_rate, "frame-size", 20, "audio-type", 2048, NULL); // 2048 is for "voip"

    // Add elements to the pipeline
    gst_bin_add_many(GST_BIN(pipeline), audio_src, convert, resample, queue, encoder, pay, sink, NULL);

    // Link the elements
    if (!gst_element_link_many(audio_src, convert, resample, queue, encoder, pay, sink, NULL)) {
        g_printerr("Failed to link GStreamer elements\n");
        gst_object_unref(pipeline);
        exit(1);
    }

}

void RtpStreamer::start(){
    // Start playing the pipeline
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to start the GStreamer pipeline\n");
        gst_object_unref(pipeline);
        exit(1);
    }
}

void RtpStreamer::stop() {
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to start the GStreamer pipeline\n");
        gst_object_unref(pipeline);
        exit(1);
    }
}