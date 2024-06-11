#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
// #include "wake_word_lib_hooks.h"
#include <string>
#include <iostream>
#include <fstream>
#include <rtp_streamer.hpp>
#include <atomic>
#include <mosquitto.h>
#include <openwakeword.hpp>
#include <chrono>
#include <rtp_receiver.hpp>

// #ifdef RASPBERRY_PI
// const char *library_path = "../wake_word_lib/porcupine/libpv_porcupine_rpi4.so";
// #else
// const char *library_path = "../wake_word_lib/porcupine/libpv_porcupine.so";
// #endif

std::atomic_bool stop_listening = false;
const uint32_t mqtt_server_port = 1883;
const std::string mqtt_server_ip = "100.72.27.109";
const std::string mqtt_topic = "status/server";

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
    printf("Received message on topic %s: %s\n", message->topic, (char *)message->payload);
    if (!strcmp((char *)message->payload, "Timeout!"))
    {
        printf("received Timeout!\n");
        stop_listening = true;
    }
}

static volatile bool is_interrupted = false;

void interrupt_handler(int _)
{
    (void)_;
    is_interrupted = true;
}

// void start_mic_capture_stream(pv_recorder_t* rec_inst){
//     fprintf(stdout, "Start recording...\n");
//     pv_recorder_status_t recorder_status = pv_recorder_start(rec_inst);
//     if (recorder_status != PV_RECORDER_STATUS_SUCCESS) {
//         fprintf(stderr, "Failed to start device with %s.\n", pv_recorder_status_to_string(recorder_status));
//         exit(1);
//     }
// }

// void get_samples_from_mic(pv_recorder_t* rec_inst, int16_t* pcm) {
//         pv_recorder_status_t recorder_status = pv_recorder_read(rec_inst, pcm);
//         if (recorder_status != PV_RECORDER_STATUS_SUCCESS)
//         {
//             fprintf(stderr, "Failed to read with %s.\n", pv_recorder_status_to_string(recorder_status));
//             exit(1);
//         }
// }

int main(int argc, char *argv[])
{
    signal(SIGINT, interrupt_handler);
    RtpStreamer rtp("100.72.27.109", 5004, 512, 48000);
    // RtpReceiver rtprecv(5002);
    mosquitto_lib_init();

    mosquitto *mosq = mosquitto_new(nullptr, true, nullptr);
    if (!mosq)
    {
        std::cerr << "Failed to create Mosquitto instance.\n";
        return 1;
    }

    int keepalive = 60;
    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, mqtt_server_ip.c_str(), mqtt_server_port, keepalive) != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Failed to connect to broker.\n";
        return 1;
    }

    mosquitto_subscribe(mosq, nullptr, mqtt_topic.c_str(), 0);
    mosquitto_loop_start(mosq);
    openwakeword_detector wakeword_detect;
    wakeword_detect.init("../model/hey_robo.onnx");
    
    while (!is_interrupted)
    {
        bool wake_word_detected = wakeword_detect.detect_wakeword();
        if (wake_word_detected)
        {
            printf("Wake word detected!\n");
            rtp.start();

        }
        if (stop_listening)
        {
            printf("Stop command received!\n");
            // rtprecv.pause();
            stop_listening = false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}
