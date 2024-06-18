#include <rtp_receiver.hpp>
#include <iostream>
#include <fstream>


RtpReceiver::RtpReceiver(int port) {
    gst_init(nullptr, nullptr);
    loop = g_main_loop_new(nullptr, FALSE);

    pipeline = gst_pipeline_new("rtp-receiver");
    udpsrc = gst_element_factory_make("udpsrc", "source");
    capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    rtpdepay = gst_element_factory_make("rtpL16depay", "depay");
    convert = gst_element_factory_make("audioconvert", "convert");
    resample = gst_element_factory_make("audioresample", "resample");
    jitterbuffer = gst_element_factory_make("rtpjitterbuffer", "jitterbuffer");
    sink = gst_element_factory_make("pulsesink", "sink");

    if (!pipeline || !udpsrc || !capsfilter || !jitterbuffer ||  !rtpdepay || !convert || !resample || !sink) {
        g_printerr("Failed to create elements.\n");
        return;
    }

    g_object_set(udpsrc, "port", port, NULL);

    GstCaps *caps = gst_caps_new_simple("application/x-rtp",
                                        "media", G_TYPE_STRING, "audio",
                                        "clock-rate", G_TYPE_INT, (int)22050,
                                        "encoding-name", G_TYPE_STRING, "L16",
                                        "channels", G_TYPE_INT, (int)1,
                                        NULL);
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(pipeline), udpsrc, capsfilter, jitterbuffer, rtpdepay, convert, resample, sink, NULL);

    if (!gst_element_link_many(udpsrc, capsfilter, jitterbuffer, rtpdepay, convert, resample, sink, NULL)) {
        g_printerr("Failed to link elements.\n");
        return;
    }

    bus = gst_element_get_bus(pipeline);
    gst_bus_add_signal_watch(bus);

    GstPad *pad = gst_element_get_static_pad(convert, "sink");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, pad_probe_callback, this, NULL);
    gst_object_unref(pad);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    // g_main_loop_run(loop);
}

GstPadProbeReturn RtpReceiver::pad_probe_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    // Check if the buffer has data
    RtpReceiver *inst = (RtpReceiver*)user_data;
    if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER) {
        GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
        if (gst_buffer_get_size(buffer) > 0) {
            inst->receivingSamples = true;
        }
    }
    return GST_PAD_PROBE_OK;
}


RtpReceiver::~RtpReceiver() {
    g_main_loop_quit(loop);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));
    g_main_loop_unref(loop);
    if(mainLoopThread.joinable()) {
        mainLoopThread.join();
    }
}

void RtpReceiver::init(){
    mainLoopThread = std::thread([this]() {
        g_main_loop_run(loop);
    });
}

bool RtpReceiver::isReceivingSamples() {
    bool receiving_ = receivingSamples;
    if(receivingSamples) {
        receivingSamples = false;
    }
    return receiving_;
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