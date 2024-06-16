#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <fstream>
#include <rtp_streamer.hpp>
#include <atomic>
#include <mosquitto.h>
#include <openwakeword.hpp>
#include <chrono>
#include <rtp_receiver.hpp>


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


int main(int argc, char *argv[])
{
    signal(SIGINT, interrupt_handler);
    RtpStreamer rtp("100.72.27.109", 5004, 512, 48000);
    RtpReceiver rtprecv(5002);
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
    rtprecv.init();
    rtprecv.resume();
    bool mic_stream_paused = false;
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
            rtp.stop();
            stop_listening = false;
        }
    }
    return 0;
}
