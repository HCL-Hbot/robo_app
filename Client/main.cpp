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
#include <boost/asio.hpp>
#include <person_detector.hpp>
#include <brainboard_host.hpp>

// #ifdef RASPBERRY_PI
// const char *library_path = "../wake_word_lib/porcupine/libpv_porcupine_rpi4.so";
// #else
// const char *library_path = "../wake_word_lib/porcupine/libpv_porcupine.so";
// #endif

std::atomic_bool stop_listening = false;
const uint32_t mqtt_server_port = 1883;
const std::string mqtt_server_ip = "100.72.27.109";
const std::string mqtt_topic = "status/server";
static volatile bool is_interrupted = false;

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
    printf("Received message on topic %s: %s\n", message->topic, (char *)message->payload);
    if (!strcmp((char *)message->payload, "Timeout!"))
    {
        printf("received Timeout!\n");
        stop_listening = true;
    }
}

void interrupt_handler(int _) {
    (void)_;
    is_interrupted = true;
}

void loadEchoCancelModule() {
    int result = std::system("pactl load-module module-echo-cancel");
    if (result != 0) {
        std::cerr << "Failed to load module-echo-cancel." << std::endl;
    } else {
        std::cout << "module-echo-cancel loaded successfully." << std::endl;
    }
}


int main(int argc, char *argv[]) {
    signal(SIGINT, interrupt_handler);
    RtpStreamer rtp("100.72.27.109", 5004, 512, 48000);
    RtpReceiver rtprecv(5002);
    // loadEchoCancelModule();

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
    BRAINBOARD_HOST::DeviceController device_controller("/dev/ttyUSB0", 115200);
    PersonDetector persondetect("127.0.0.1", "5678", device_controller);
    persondetect.init();
/* End of Person Tracking Subsystem: */

/* Audio Interaction Subsystem: */
    openwakeword_detector wakeword_detect;
    wakeword_detect.init("../model/hey_robo.onnx");
    rtprecv.init();
    rtprecv.resume();
/* End of Audio Interaction Subsystem: */

    // Start timer for Robo blinking:
    auto start_time = std::chrono::steady_clock::now(); 
    while (! is_interrupted) {
        bool wake_word_detected = wakeword_detect.detect_wakeword();
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
            printf("1 second has passed.\n");
            device_controller.controlEyes(BRAINBOARD_HOST::EyeID::BOTH, 0, 0, BRAINBOARD_HOST::EyeAnimation::BLINK);
            // Reset the start time
            start_time = current_time;
        }

        // if () {
        //     std::string person = persondetect.detect_person();
        //     if (person == "person") {
        //         printf("Person detected!\n");
        //         rtp.start();
        //     }
        // }
    }
    return 0;
}
