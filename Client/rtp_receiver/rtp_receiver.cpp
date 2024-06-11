#include <rtp_receiver.hpp>
#include <iostream>
#include <fstream>


RtpReceiver::RtpReceiver(int port) {
     gst_init(nullptr, nullptr);
    loop = g_main_loop_new(nullptr, FALSE);

    pipeline = gst_pipeline_new("rtp-receiver");
    udpsrc = gst_element_factory_make("udpsrc", "source");
    rtpdepay = gst_element_factory_make("rtpL16depay", "depay");
    capsfilter = gst_element_factory_make("capsfilter", "filter");
    convert = gst_element_factory_make("audioconvert", "convert");
    resample = gst_element_factory_make("audioresample", "resample");
    sink = gst_element_factory_make("autoaudiosink", "sink");

    if (!pipeline || !udpsrc || !rtpdepay || !capsfilter || !convert || !resample || !sink) {
        g_printerr("Failed to create elements.\n");
        return;
    }

    g_object_set(udpsrc, "port", port, NULL);
    
    GstCaps *caps = gst_caps_new_simple("application/x-rtp",
                                        "media", G_TYPE_STRING, "audio",
                                        "clock-rate", G_TYPE_INT, 22050,
                                        "encoding-name", G_TYPE_STRING, "L16",
                                        NULL);
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(pipeline), udpsrc, rtpdepay, capsfilter, convert, resample, sink, NULL);
    if (!gst_element_link_many(udpsrc, rtpdepay, capsfilter, convert, resample, sink, NULL)) {
        g_printerr("Failed to link elements.\n");
        return;
    }

    bus = gst_element_get_bus(pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message::error", G_CALLBACK(+[](GstBus *bus, GstMessage *msg, gpointer user_data) {
        GError *err;
        gchar *debug_info;
        gst_message_parse_error(msg, &err, &debug_info);
        g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
        g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
        g_clear_error(&err);
        g_free(debug_info);
        g_main_loop_quit((GMainLoop *)user_data);
    }), loop);

    g_signal_connect(bus, "message::eos", G_CALLBACK(+[](GstBus *bus, GstMessage *msg, gpointer user_data) {
        g_main_loop_quit((GMainLoop *)user_data);
    }), loop);
}

RtpReceiver::~RtpReceiver() {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));
    g_main_loop_unref(loop);
}

bool RtpReceiver::isReceivingSamples() {
    GstState state;
    GstState pending;
    GstStateChangeReturn ret = gst_element_get_state(pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
    return (ret == GST_STATE_CHANGE_SUCCESS || ret == GST_STATE_CHANGE_ASYNC) &&
           (state == GST_STATE_PLAYING || state == GST_STATE_PAUSED);
}

void RtpReceiver::pause() {
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to pause the GStreamer pipeline\n");
    } else {
        g_print("Pipeline paused\n");
    }
}

void RtpReceiver::resume() {
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to resume the GStreamer pipeline\n");
    } else {
        g_print("Pipeline resumed\n");
    }
}