#include "audio_buffer.hpp"
#include "common.h"
#include "whisper.h"
#include "llama.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include <regex>
#include <sstream>
#include <mosquitto.h>
#include <iostream>
#include <whisper_helper.hpp>
#include <llama_wrapper.hpp>
#include <whisper_wrapper.hpp>

const uint32_t mqtt_server_port = 1883;
const std::string mqtt_server_ip = "";
const std::string mqtt_up_msg = "Server is ready!";
const std::string mqtt_listening_msg = "Server is listening!";
const std::string audio_timeout_msg = "Timeout!";
const std::string mqtt_topic = "status/server";

const float timeout_time = 4.5f;

int main(int argc, char **argv)
{
    whisper_params params;

    whisper_wrapper whisper_inst(params);

    llama_wrapper llama_inst(params);
    int port = 5004;
    std::shared_ptr<RtpReceiver> Receiver = std::make_shared<RtpReceiver>(port);
    audio_async audio(Receiver, 30 * 1000);
    if (!audio.init())
    {
        fprintf(stderr, "%s: audio.init() failed!\n", __func__);
        return 1;
    }

    audio.resume();

    bool is_running = true;

    std::vector<float> pcmf32_cur;

    whisper_inst.init();
    // init session
    llama_inst.init();

    // clear audio buffer
    audio.clear();

    int keepalive = 60;

    mosquitto_lib_init();

    mosquitto *mosq = mosquitto_new(nullptr, true, nullptr);
    if (!mosq)
    {
        std::cerr << "Failed to create Mosquitto instance.\n";
        return 1;
    }

    if (mosquitto_connect(mosq, mqtt_server_ip.c_str(), mqtt_server_port, keepalive) != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Failed to connect to broker.\n";
        return 1;
    }

    if (mosquitto_publish(mosq, nullptr, mqtt_topic.c_str(), mqtt_up_msg.length(), mqtt_up_msg.c_str(), 0, false) != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Failed to publish message.\n";
        return 1;
    }

    bool force_speak = false;
    bool prev_active_audio = false;
    std::chrono::steady_clock::time_point listening_start_time;

    // main loop
    while (is_running)
    {
        // handle Ctrl + C
        is_running = sdl_poll_events();

        if (!is_running)
        {
            break;
        }

        // delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        int64_t t_ms = 0;

        {
            if (audio.active() && !prev_active_audio)
            {
                listening_start_time = std::chrono::steady_clock::now();
                prev_active_audio = true;
            }
            else if (!audio.active() && prev_active_audio)
            {
                prev_active_audio = false;
            }
            else if (audio.active() && prev_active_audio)
            {
                auto now = std::chrono::steady_clock::now();
                std::chrono::duration<double> elapsed_seconds = now - listening_start_time;
                std::cout << "Elapsed time: " << elapsed_seconds.count() << "\n";
                if (elapsed_seconds.count() > timeout_time)
                {
                    std::cout << "timeout!" << '\n';
                    if (mosquitto_publish(mosq, nullptr, mqtt_topic.c_str(), audio_timeout_msg.length(), audio_timeout_msg.c_str(), 0, false) != MOSQ_ERR_SUCCESS)
                    {
                        std::cerr << "Failed to publish message.\n";
                        return 1;
                    }
                }
            }

            audio.get(2000, pcmf32_cur);
            if (::vad_simple(pcmf32_cur, WHISPER_SAMPLE_RATE, 1250, params.vad_thold, params.freq_thold, params.print_energy) || force_speak)
            {
                // fprintf(stdout, "%s: Speech detected! Processing ...\n", __func__);
                if (mosquitto_publish(mosq, nullptr, mqtt_topic.c_str(), mqtt_listening_msg.length(), mqtt_listening_msg.c_str(), 0, false) != MOSQ_ERR_SUCCESS)
                {
                    std::cerr << "Failed to publish message.\n";
                    return 1;
                }
                audio.get(params.voice_ms, pcmf32_cur);

                std::string text_heard = whisper_inst.do_inference(pcmf32_cur, t_ms, prev_active_audio);

                force_speak = false;

                if (text_heard.empty() || force_speak)
                {
                    audio.clear();
                    continue;
                }
                fprintf(stdout, "%s%s%s", "\033[1m", text_heard.c_str(), "\033[0m");
                fflush(stdout);

                std::string text_to_speak = llama_inst.do_inference(text_heard);

                speak_with_file(params.speak, text_to_speak, params.speak_file, 2);

                audio.clear();
            }
        }
    }

    audio.pause();

    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}