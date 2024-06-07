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
#include <iostream>
#include <whisper_helper.hpp>
#include <llama_wrapper.hpp>
#include <whisper_wrapper.hpp>
#include <mqtt_helpers/mqtt_helper.hpp>


const float timeout_time = 4.5f;

void send_timeout_msg(mosquitto *mosq, std::chrono::steady_clock::time_point &audio_start_time, bool audio_is_now_active, bool &audio_was_active)
{
    if (audio_is_now_active && !audio_was_active)
    {
        audio_start_time = std::chrono::steady_clock::now();
        audio_was_active = true;
    }
    else if (!audio_is_now_active && audio_was_active)
    {
        audio_was_active = false;
    }
    else if (audio_is_now_active && audio_was_active)
    {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = now - audio_start_time;
        std::cout << "Elapsed time: " << elapsed_seconds.count() << "\n";
        if (elapsed_seconds.count() > timeout_time)
        {
            std::cout << "timeout!" << '\n';
            mqtt_send_msg(mosq, mqtt_topic, audio_timeout_msg);
        }
    }
}

int main(int argc, char **argv)
{

    mosquitto *mosq = mqtt_init();
    
    const uint32_t mqtt_server_port = 1883;
    const std::string mqtt_server_ip = "";

    mqtt_connect(mosq, mqtt_server_ip, mqtt_server_port);

    mqtt_send_msg(mosq, mqtt_topic, mqtt_up_msg);


    whisper_params params;

    whisper_wrapper whisper_inst(params);

    llama_wrapper llama_inst(params);

    int rtp_recv_port = 5004;
    std::shared_ptr<RtpReceiver> Receiver = std::make_shared<RtpReceiver>(rtp_recv_port);
    audio_async audio(Receiver, 30 * 1000);
    if (!audio.init())
    {
        fprintf(stderr, "%s: audio.init() failed!\n", __func__);
        return 1;
    }

    audio.resume();

    // clear audio buffer
    audio.clear();

    whisper_inst.init();

    llama_inst.init();

    bool is_running = true;
    bool audio_was_active = false;
    std::vector<float> pcmf32_cur;
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
            send_timeout_msg(mosq, listening_start_time, audio.active(), audio_was_active);

            audio.get(2000, pcmf32_cur);
            if (::vad_simple(pcmf32_cur, WHISPER_SAMPLE_RATE, 1250, params.vad_thold, params.freq_thold, params.print_energy))
            {
                mqtt_send_msg(mosq, mqtt_topic, mqtt_listening_msg);
                audio.get(params.voice_ms, pcmf32_cur);

                std::string text_heard = whisper_inst.do_inference(pcmf32_cur, t_ms);

                if (text_heard.empty())
                {
                    /* No words were captured! Continue; */
                    audio.clear();
                    continue;
                } else {
                    /* Words were captured! Reset the timeout timer! */
                    audio_was_active = false;
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