#include <rtp_receiver.hpp>
#include <iostream>
#include <fstream>


RtpReceiver::RtpReceiver(int port) {
    gst_init(NULL, NULL);

    pipeline = gst_pipeline_new("audio-receiver");
    udpsrc = gst_element_factory_make("udpsrc", "udp-source");
    rtpdepay = gst_element_factory_make("rtpopusdepay", "rtp-depay");
    decoder = gst_element_factory_make("opusdec", "decoder");
    convert = gst_element_factory_make("audioconvert", "converter");
    resample = gst_element_factory_make("audioresample", "resample");
    capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    sink = gst_element_factory_make("appsink", "app-sink");

    if (!pipeline || !udpsrc || !rtpdepay || !decoder || !convert || !resample || !capsfilter || !sink) {
        g_printerr("Failed to create GStreamer elements\n");
        exit(1);
    }

    // Set properties
    g_object_set(G_OBJECT(udpsrc), "port", port, NULL);

    // Set caps for udpsrc
    GstCaps *caps = gst_caps_new_simple("application/x-rtp",
                                        "media", G_TYPE_STRING, "audio",
                                        "clock-rate", G_TYPE_INT, 48000,
                                        "encoding-name", G_TYPE_STRING, "OPUS",
                                        "payload", G_TYPE_INT, 96,
                                        NULL);
    g_object_set(udpsrc, "caps", caps, NULL);
    gst_caps_unref(caps);

    // Set caps for resampling to 16 kHz mono
    GstCaps *resample_caps = gst_caps_new_simple("audio/x-raw",
                                                 "format", G_TYPE_STRING, "F32LE",
                                                 "rate", G_TYPE_INT, 16000,
                                                 "channels", G_TYPE_INT, 1,
                                                 NULL);
    g_object_set(capsfilter, "caps", resample_caps, NULL);
    gst_caps_unref(resample_caps);

    // Configure appsink for 16 kHz mono
    g_object_set(G_OBJECT(sink), "emit-signals", TRUE, "sync", FALSE, "caps",
                 gst_caps_new_simple("audio/x-raw",
                                     "format", G_TYPE_STRING, "F32LE",
                                     "rate", G_TYPE_INT, 16000,
                                     "channels", G_TYPE_INT, 1,
                                     NULL), NULL);

    // Add elements to the pipeline
    gst_bin_add_many(GST_BIN(pipeline), udpsrc, rtpdepay, decoder, convert, resample, capsfilter, sink, NULL);

    // Link the elements
    if (!gst_element_link_many(udpsrc, rtpdepay, decoder, convert, resample, capsfilter, sink, NULL)) {
        g_printerr("Failed to link GStreamer elements\n");
        gst_object_unref(pipeline);
        exit(1);
    }

    // Create the main loop
    loop = g_main_loop_new(NULL, FALSE);
}

RtpReceiver::~RtpReceiver() {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));
    g_main_loop_unref(loop);
}

void RtpReceiver::start(SampleCallback callback) {
    // Store the callback
    sample_callback = callback;

    // Connect to the appsink signal
    g_signal_connect(sink, "new-sample", G_CALLBACK(+[](GstElement *sink, gpointer data) -> GstFlowReturn {
        RtpReceiver *receiver = static_cast<RtpReceiver*>(data);
        GstSample *sample = NULL;
        g_signal_emit_by_name(sink, "pull-sample", &sample);
        if (sample) {
            GstBuffer *buffer = gst_sample_get_buffer(sample);
            GstMapInfo info;
            if (gst_buffer_map(buffer, &info, GST_MAP_READ)) {
                // Call the user-provided callback with the PCM 32-bit float samples
                receiver->sample_callback(info.data, info.size);
                gst_buffer_unmap(buffer, &info);
            }
            gst_sample_unref(sample);
            return GST_FLOW_OK;
        }
        return GST_FLOW_ERROR;
    }), this);
   
}

void RtpReceiver::stop() {
    g_main_loop_quit(loop);
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