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
#include <async_audio_buffer.hpp>
#include <openwakeword.hpp>
#include <chrono>
#include <rtp_receiver.hpp>
#include <boost/asio.hpp>
#include <person_detector.hpp>
#include <brainboard_host.hpp>

/* Local Prototypes */
void interrupt_handler(int _);
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
void loadEchoCancelModule();

std::atomic_bool stop_listening = false;
const uint32_t mqtt_server_port = 1883;
const std::string mqtt_server_ip = "100.72.27.109";
const std::string mqtt_topic = "status/server";
static volatile bool is_interrupted = false;

int main(int argc, char *argv[]) {
    signal(SIGINT, interrupt_handler);
    RtpStreamer rtp("100.72.27.109", 5004, 512, 48000);
    RtpReceiver rtprecv(5002);
    /* Async audio capture for wakeword detection */
    audio_async audio(30 * 1000);
    audio.init(-1, 16000);

/* MQTT Setup */
    const int keepalive = 60;
    mosquitto_lib_init();
    
    mosquitto *mosq = mosquitto_new(nullptr, true, nullptr);
    if (! mosq) {
        std::cerr << "Failed to create Mosquitto instance.\n";
        return 1;
    }
    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, mqtt_server_ip.c_str(), mqtt_server_port, keepalive) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to connect to broker.\n";
        return 1;
    }

    mosquitto_subscribe(mosq, nullptr, mqtt_topic.c_str(), 0);
    mosquitto_loop_start(mosq);
/* End of MQTT Setup */

/* Person Tracking Subsystem: */
    BRAINBOARD_HOST::DeviceController device_controller("/dev/ttyS0", 115200);
    PersonDetector persondetect("127.0.0.1", "5678", device_controller);
    persondetect.init();
/* End of Person Tracking Subsystem: */

/* Audio Interaction Subsystem: */
    openwakeword_detector wakeword_detect;
    wakeword_detect.init("../model/hey_robo.onnx");
    rtprecv.init();
    rtprecv.resume();
/* End of Audio Interaction Subsystem: */

/* Local audio buffer for the wakeword detection */
    std::vector<float> audio_samples(1024);

    audio.resume();
    audio.clear();

    // Start timer for Robo blinking:
    auto start_time = std::chrono::steady_clock::now(); 
    while (!is_interrupted) {
        bool wake_word_detected = wakeword_detect.detect_wakeword(audio_samples);
        if (wake_word_detected) {
            printf("Wake word detected!\n");
            device_controller.setColor(BRAINBOARD_HOST::LedID::HEAD, BRAINBOARD_HOST::Color::WHITE);
            rtp.start();
        }
        if (stop_listening) {
            printf("Stop command received!\n");
            rtp.stop();
            stop_listening = false;
        }

        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();

        if (elapsed_time > 1000 * 30) {
            // Every 30 sec Robo blinks.
            printf("30 second has passed.\n");
            device_controller.blink(BRAINBOARD_HOST::EyeID::BOTH);
            // Reset the start time
            start_time = current_time;
        }

    }
    return 0;
}

/* Interrupt handler for disabling the sys */
void interrupt_handler(int _) {
    (void)_;
    is_interrupted = true;
}

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    printf("Received message on topic %s: %s\n", message->topic, (char *)message->payload);
    if (!strcmp((char *)message->payload, "Timeout!")) {
        printf("received Timeout!\n");
        stop_listening = true;
    }
}
