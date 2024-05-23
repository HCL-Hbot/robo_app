#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "wake_word_lib_hooks.h"
#include <string>
#include <iostream>
#include <fstream>
#include <rtp_streamer.hpp>

const char *library_path = "../wake_word_lib/porcupine/libpv_porcupine.so";
const char *api_key = "***REMOVED***";
const char *model_path = "../wake_word_lib/porcupine_params.pv";
const char *keyword_path = "../wake_word_lib/model/hey_quiri_x86_64.ppn";


static volatile bool is_interrupted = false;

void interrupt_handler(int _) {
    (void) _;
    is_interrupted = true;
}

void start_mic_capture_stream(pv_recorder_t* rec_inst){
    fprintf(stdout, "Start recording...\n");
    pv_recorder_status_t recorder_status = pv_recorder_start(rec_inst);
    if (recorder_status != PV_RECORDER_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to start device with %s.\n", pv_recorder_status_to_string(recorder_status));
        exit(1);
    }
}

void get_samples_from_mic(pv_recorder_t* rec_inst, int16_t* pcm) {
        pv_recorder_status_t recorder_status = pv_recorder_read(rec_inst, pcm);
        if (recorder_status != PV_RECORDER_STATUS_SUCCESS)
        {
            fprintf(stderr, "Failed to read with %s.\n", pv_recorder_status_to_string(recorder_status));
            exit(1);
        }
}

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
    wake_word_lib wake_word_detector(-1, library_path, api_key, model_path, keyword_path);
    RtpStreamer rtp("127.0.0.1", 5004, 512, 48000);

    start_mic_capture_stream(wake_word_detector.get_recorder_inst());

    uint32_t frame_length = wake_word_detector.get_frame_length();
    int16_t *pcm = (int16_t*)malloc(frame_length * sizeof(int16_t));
    printf("%d\n", frame_length);
    if (!pcm) {
        fprintf(stderr, "Failed to allocate pcm memory.\n");
        exit(1);
    }
    
    pv_recorder_t *recorder = wake_word_detector.get_recorder_inst();
    while(!is_interrupted) {
        get_samples_from_mic(recorder, pcm);
        int wake_word_detected = wake_word_detector.detect_wakeword(pcm);
        if(wake_word_detected) {
            printf("Wake word detected!\n");
            rtp.start();
        }
    }
    free(pcm);
    pv_recorder_delete(recorder);
    return 0;
}
