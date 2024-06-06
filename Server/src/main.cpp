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

    struct whisper_context_params cparams = whisper_context_default_params();

    cparams.use_gpu = params.use_gpu;
    cparams.flash_attn = params.flash_attn;

    struct whisper_context *ctx_wsp = whisper_init_from_file_with_params(params.model_wsp.c_str(), cparams);
    if (!ctx_wsp)
    {
        fprintf(stderr, "No whisper.cpp model specified. Please provide using -mw <modelfile>\n");
        return 1;
    }
    
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
    bool force_speak = false;

    float prob0 = 0.0f;

    const std::string chat_symb = ":";

    std::vector<float> pcmf32_cur;
    std::vector<float> pcmf32_prompt;

    const std::string prompt_whisper = ::replace(k_prompt_whisper, "{1}", params.bot_name);

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

                std::string all_heard;

                if (!force_speak)
                {
                    all_heard = ::trim(::transcribe(ctx_wsp, params, pcmf32_cur, prompt_whisper, prob0, t_ms));
                }

                const auto words = get_words(all_heard);

                std::string wake_cmd_heard;
                std::string text_heard;

                if (words.size() > 0)
                {
                    prev_active_audio = false;
                }

                for (int i = 0; i < (int)words.size(); ++i)
                {
                    text_heard += words[i] + " ";
                }

                // check if audio starts with the wake-up command if enabled

                // optionally give audio feedback that the current text is being processed
                if (!params.heard_ok.empty())
                {
                    speak_with_file(params.speak, params.heard_ok, params.speak_file, 2);
                }

                // remove text between brackets using regex
                {
                    std::regex re("\\[.*?\\]");
                    text_heard = std::regex_replace(text_heard, re, "");
                }

                // remove text between brackets using regex
                {
                    std::regex re("\\(.*?\\)");
                    text_heard = std::regex_replace(text_heard, re, "");
                }

                // remove all characters, except for letters, numbers, punctuation and ':', '\'', '-', ' '
                text_heard = std::regex_replace(text_heard, std::regex("[^a-zA-Z0-9\\.,\\?!\\s\\:\\'\\-]"), "");

                // take first line
                text_heard = text_heard.substr(0, text_heard.find_first_of('\n'));

                // remove leading and trailing whitespace
                text_heard = std::regex_replace(text_heard, std::regex("^\\s+"), "");
                text_heard = std::regex_replace(text_heard, std::regex("\\s+$"), "");

                force_speak = false;

                if (text_heard.empty() || force_speak)
                {
                    audio.clear();
                    continue;
                }
                fprintf(stdout, "%s%s%s", "\033[1m", text_heard.c_str(), "\033[0m");
                fflush(stdout);

                std::string text_to_speak = llama_inst.do_inference(text_heard, t_ms);

                speak_with_file(params.speak, text_to_speak, params.speak_file, 2);

                audio.clear();
            }
        }
    }

    audio.pause();

    whisper_print_timings(ctx_wsp);
    whisper_free(ctx_wsp);

    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}