#include <iostream>
#include <settings.hpp>
#include "common.h"
#include "audio_buffer.hpp"
#include <whisper_helper.hpp>
#include <llama_wrapper.hpp>
#include <whisper_wrapper.hpp>
#include <mqtt_helpers/mqtt_helper.hpp>

void send_timeout_msg(mosquitto *mosq, std::chrono::steady_clock::time_point &audio_start_time, bool audio_is_now_active, bool &audio_was_active)
{
    if (audio_is_now_active && !audio_was_active)
    {
        /* This condition is at the start and when timer is freshly reset with new audio samples coming in */
        /* Update the start_time timestamp, and set audio_was_active to false, so this condition does not occur till the timer is reset*/
        audio_start_time = std::chrono::steady_clock::now();
        audio_was_active = true;
    }
    else if (!audio_is_now_active && audio_was_active)
    {
        /* Audio rtp connection dropped or client disconnected, reset the timer! */
        audio_was_active = false;
    }
    else if (audio_is_now_active && audio_was_active)
    {
        /* Audio is actively sampling and we were already sampling audio, compare the timestamp and decide whether timeout has been reached*/
        auto now = std::chrono::steady_clock::now();

        std::chrono::duration<double> elapsed_seconds = now - audio_start_time;

        std::cout << "Elapsed time: " << elapsed_seconds.count() << "\n";
        if (elapsed_seconds.count() > timeout_time)
        {
            /* Timeout has been reached! */
            std::cout << "timeout!" << '\n';
            mqtt_send_msg(mosq, mqtt_topic, audio_timeout_msg);
        }
    }
}

int main(int argc, char **argv)
{
    /* 
    * MQTT 
    * Used for communication to any connected clients *
    * This section initialzes MQTT and sends a message to clients telling that the server is up! 
    */
    mosquitto *mosq = mqtt_init();

    mqtt_connect(mosq, mqtt_server_ip, mqtt_server_port);

    mqtt_send_msg(mosq, mqtt_topic, mqtt_up_msg);

   /* 
    * Whisper_params                                                                  
    *                                                                                                                                                    
    * This is the struct which holds all the settings for the llama and whisper model 
    * By default it contains valid settings; But feel free to change any of the settings 
    * The definition of this struct is in settings.hpp 
    */
    model_params params;

    /* Create instance of whisper_wrapper
     * This class loads in and manages the whisper model 
     */
    whisper_wrapper whisper_inst(params);

    /* Create instance of llama_wrapper
    * This class loads in and manages the llama model 
    */
    llama_wrapper llama_inst(params);

    /* 
     * Create a new instance of the RTP receiver that will receive audio samples over RTP from the client 
     */
    std::shared_ptr<RtpReceiver> Receiver = std::make_shared<RtpReceiver>(rtp_recv_port);

    /*
     * Create the async audio buffer with a instance of the RTP receiver so that the RTP receiver automatically updates the audio buffer
    */
    audio_async audio(Receiver, audio_buffer_size_ms);
    if (!audio.init())
    {
        fprintf(stderr, "%s: audio.init() failed!\n", __func__);
        return 1;
    }

    // Start the audio capture
    audio.resume();

    // clear audio buffer
    audio.clear();

    /* This function call will load in the whisper model 
    * It will also do a test run to calibrate the response of the model
    * The test run also prepares the model for continuous operation 
    */
    whisper_inst.init();

    /* This function call will load in the llama model
    * It will also do a test run to calibrate the response of the model (some tokenizening and stuff)
    * The test run also prepares the model for continuous operation 
    */
    llama_inst.init();

    /* This bool get's toggled when CTRL+C is entered */
    bool is_running = true;

    /* Buffer which holds the audio sensors that will be captured with the audio.get functions */
    std::vector<float> pcmf32_cur;

    /* Time point which is used to keep track of silences in the audio, to stop listening after n amount of time */
    std::chrono::steady_clock::time_point listening_start_time;

    /* Keeps track of new audio samples were coming in previously, it get's compared with current state of audio sampling buffer
    * So that the timer can keep track of it's state and reset when necessary 
    */
    bool audio_was_active = false;
    
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

        {
            /* Run the timeout timer and send a timeout message to the client when timeout threshold is reached */
            send_timeout_msg(mosq, listening_start_time, audio.active(), audio_was_active);

            /* Non-blocking call to get 32-bit floating point pcm samples from circular audio buffer
            * The default amount of samples to be fetched is around 2000ms, that is enough to fetch one or multiple words 
            */
            audio.get(2000, pcmf32_cur);
            
            /* Detect audio activity using vad; A algorithm that determines voice activity by applying a band-pass filter and looking at the energy of the audio captured
            * Want to learn more? See: https://speechprocessingbook.aalto.fi/Recognition/Voice_activity_detection.html 
            */
            bool voice_activity_detected = ::vad_simple(pcmf32_cur, WHISPER_SAMPLE_RATE, 1250, params.vad_thold, params.freq_thold, params.print_energy);
            if (voice_activity_detected)
            {
                /* Send a message to the client indicating that we are listening for incoming words; The client may hook-up an animation or activity to this */
                mqtt_send_msg(mosq, mqtt_topic, mqtt_listening_msg);

                /* Capture some more audio, to make sure we capture a full sentence, the amount of time to be captured is defined in the model_params struct */
                audio.get(params.voice_ms, pcmf32_cur);

                /* Run inference with whisper on the captured audio, this will output the sentence that was captured from the audio */
                std::string text_heard = whisper_inst.do_inference(pcmf32_cur);

                /* Was there any text returned from the whisper inference? If no text, we obviously don't want to run llama on it!*/
                if (text_heard.empty())
                {
                    /* No words were captured! Continue with capturing new audio; */
                    audio.clear();
                    continue;
                } else {
                    /* Words were captured! Reset the timeout timer! */
                    audio_was_active = false;
                }
                
                /* Print the text we got from whisper inference  */
                fprintf(stdout, "%s%s%s", "\033[1m", text_heard.c_str(), "\033[0m");
                fflush(stdout);

                /* Run llama inference */
                std::string text_to_speak = llama_inst.do_inference(text_heard);

                /* Run an external script to convert the text back to speech and stream it back to the client */
                speak_with_file(params.speak, text_to_speak, params.speak_file, 2);

                /* Empty the audio buffer, so that we do not process the same audio twice! */
                audio.clear();
            }
        }
    }
    
    /* Stop with audio capturing */
    audio.pause();

    /* Disconnect and clean up the mqtt client */
    clean_up_mqtt(mosq);

    return 0;
}