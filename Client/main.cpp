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
#include "model/model.h"
#include <openwakeword.hpp>

// #ifdef RASPBERRY_PI
// const char *library_path = "../wake_word_lib/porcupine/libpv_porcupine_rpi4.so";
// #else
// const char *library_path = "../wake_word_lib/porcupine/libpv_porcupine.so";
// #endif


std::atomic_bool stop_listening = false;
const uint32_t mqtt_server_port = 1883;
const std::string mqtt_server_ip = "";
const std::string mqtt_topic = "status/server";

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    printf("Received message on topic %s: %s\n", message->topic, (char*) message->payload);
    if(!strcmp((char*)message->payload, "Timeout!")) {
        printf("received Timeout!\n");
        stop_listening = true;
    }
}

static volatile bool is_interrupted = false;

void interrupt_handler(int _) {
    (void) _;
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

void writePCMToFile(const std::string& filename, const int16_t* pcmData, size_t dataSize, bool append = false) {
    std::ios_base::openmode mode = std::ios::binary;
    if (append) {
        mode |= std::ios::app;
    } else {
        mode |= std::ios::out;
    }

    std::ofstream outFile(filename, mode);  // Open file in binary mode, with append option if specified
    if (!outFile) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return;
    }

    // Write PCM data to file
    outFile.write(reinterpret_cast<const char*>(pcmData), dataSize * sizeof(int16_t));
    if (!outFile) {
        std::cerr << "Failed to write data to file: " << filename << std::endl;
    }

    outFile.close();  // Close the file
}

int main(int argc, char *argv[]) {
    signal(SIGINT, interrupt_handler);
    // wake_word_lib wake_word_detector(-1, library_path, api_key, model_path, keyword_path);
    RtpStreamer rtp("127.0.0.1", 5004, 512, 48000);

    mosquitto_lib_init();

    mosquitto *mosq = mosquitto_new(nullptr, true, nullptr);
    if (!mosq) {
        std::cerr << "Failed to create Mosquitto instance.\n";
        return 1;
    }
    
    int keepalive = 60;
    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, mqtt_server_ip.c_str(), mqtt_server_port, keepalive) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to connect to broker.\n";
        return 1;
    }

    mosquitto_subscribe(mosq, nullptr, mqtt_topic.c_str(), 0);
    mosquitto_loop_start(mosq);
    openwakeword_detector("models/hey_robo.onnx");

    // start_mic_capture_stream(wake_word_detector.get_recorder_inst());

    // uint32_t frame_length = wake_word_detector.get_frame_length();
    // int16_t *pcm = (int16_t*)malloc(frame_length * sizeof(int16_t));
    // printf("%d\n", frame_length);
    // if (!pcm) {
    //     fprintf(stderr, "Failed to allocate pcm memory.\n");
    //     exit(1);
    // }
    
    // pv_recorder_t *recorder = wake_word_detector.get_recorder_inst();
    while(!is_interrupted) {
        // get_samples_from_mic(recorder, pcm);
        // int wake_word_detected = wake_word_detector.detect_wakeword(pcm);
        // if(wake_word_detected) {
        //     printf("Wake word detected!\n");
        //     rtp.start();
        // }
        // if(stop_listening) {
        //     printf("Stop command received!\n");
        //     rtp.stop();
        //     stop_listening = false;
        // }
    }
    // free(pcm);
    // pv_recorder_delete(recorder);
    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}
