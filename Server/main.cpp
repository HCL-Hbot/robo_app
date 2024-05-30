#include <stdio.h>
#include <rtp_receiver.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <whisper.h>
#include "common.h"
#include <audio_buffer.hpp>
#include <regex>

// command-line parameters
struct whisper_params
{
	int32_t n_threads = std::min(4, (int32_t)std::thread::hardware_concurrency());
	int32_t voice_ms = 10000;
	int32_t capture_id = -1;
	int32_t max_tokens = 32;
	int32_t audio_ctx = 0;

	float vad_thold = 0.6f;
	float freq_thold = 100.0f;

	bool speed_up = false;
	bool translate = false;
	bool print_special = false;
	bool print_energy = false;
	bool no_timestamps = true;
	bool use_gpu = true;
	bool flash_attn = false;

	std::string person = "Santa";
	std::string language = "nl";
	std::string model_wsp = "../models/ggml-medium.bin";
	std::string fname_out;
};

void writePCMToFile(const std::string &filename, const float *pcmData, size_t dataSize, bool append = false)
{
	std::ios_base::openmode mode = std::ios::binary;
	if (append)
	{
		mode |= std::ios::app;
	}
	else
	{
		mode |= std::ios::out;
	}

	std::ofstream outFile(filename, mode); // Open file in binary mode, with append option if specified
	if (!outFile)
	{
		std::cerr << "Failed to open file for writing: " << filename << std::endl;
		return;
	}

	// Write PCM data to file
	outFile.write(reinterpret_cast<const char *>(pcmData), dataSize * sizeof(float));
	if (!outFile)
	{
		std::cerr << "Failed to write data to file: " << filename << std::endl;
	}

	outFile.close(); // Close the file
}


std::string transcribe(whisper_context *ctx, const whisper_params &params, const std::vector<float> &pcmf32, float &prob, int64_t &t_ms)
{
	const auto t_start = std::chrono::high_resolution_clock::now();

	prob = 0.0f;
	t_ms = 0;

	whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

	wparams.print_progress = false;
	wparams.print_special = params.print_special;
	wparams.print_realtime = false;
	wparams.print_timestamps = !params.no_timestamps;
	wparams.translate = params.translate;
	wparams.no_context = true;
	wparams.single_segment = true;
	wparams.max_tokens = params.max_tokens;
	wparams.language = params.language.c_str();
	wparams.n_threads = params.n_threads;

	wparams.audio_ctx = params.audio_ctx;
	wparams.speed_up = params.speed_up;

	if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0)
	{
		return "";
	}

	int prob_n = 0;
	std::string result;

	const int n_segments = whisper_full_n_segments(ctx);
	for (int i = 0; i < n_segments; ++i)
	{
		const char *text = whisper_full_get_segment_text(ctx, i);

		result += text;

		const int n_tokens = whisper_full_n_tokens(ctx, i);
		for (int j = 0; j < n_tokens; ++j)
		{
			const auto token = whisper_full_get_token_data(ctx, i, j);

			prob += token.p;
			++prob_n;
		}
	}

	if (prob_n > 0)
	{
		prob /= prob_n;
	}

	const auto t_end = std::chrono::high_resolution_clock::now();
	t_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();

	return result;
}

std::vector<std::string> get_words(const std::string &txt) {
    std::vector<std::string> words;

    std::istringstream iss(txt);
    std::string word;
    while (iss >> word) {
        words.push_back(word);
    }

    return words;
}

void whisper_inference_thread()
{
	int port = 5004;
	std::shared_ptr<RtpReceiver> Receiver = std::make_shared<RtpReceiver>(port);
	std::shared_ptr<audio_async> audio = std::make_shared<audio_async>(Receiver, 30 * 1000);
	audio->init();
	whisper_params params;
	// whisper init
	struct whisper_context_params cparams = whisper_context_default_params();

	cparams.use_gpu = params.use_gpu;
	cparams.flash_attn = params.flash_attn;

	struct whisper_context *ctx_wsp = whisper_init_from_file_with_params(params.model_wsp.c_str(), cparams);
	{
		fprintf(stderr, "\n");
		if (!whisper_is_multilingual(ctx_wsp))
		{
			if (params.language != "en" || params.translate)
			{
				params.language = "en";
				params.translate = false;
				fprintf(stderr, "%s: WARNING: model is not multilingual, ignoring language and translation options\n", __func__);
			}
		}
		fprintf(stderr, "%s: processing, %d threads, lang = %s, task = %s, timestamps = %d ...\n",
				__func__,
				params.n_threads,
				params.language.c_str(),
				params.translate ? "translate" : "transcribe",
				params.no_timestamps ? 0 : 1);

		fprintf(stderr, "\n");
	}
	float prob0 = 0.0f;
	bool force_speak = false;
	int i = 0;
	std::vector<float> buffer;
	int threshold = 50;
	audio->resume();
	while (1)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

        int64_t t_ms = 0;

        {
            audio->get(2000, buffer);
			writePCMToFile("test.pcm", buffer.data(), buffer.size(), true);
            if (::vad_simple(buffer, WHISPER_SAMPLE_RATE, 1250, params.vad_thold, params.freq_thold, params.print_energy) || force_speak) {
                fprintf(stdout, "%s: Speech detected! Processing ...\n", __func__);

                audio->get(params.voice_ms, buffer);

                std::string all_heard;

                if (!force_speak) {
                    all_heard = ::trim(::transcribe(ctx_wsp, params, buffer, prob0, t_ms));
                }

                const auto words = get_words(all_heard);

                std::string wake_cmd_heard;
                std::string text_heard;

                for (int i = 0; i < (int) words.size(); ++i) {
                        text_heard += words[i] + " ";
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

                text_heard.insert(0, 1, ' ');
                fprintf(stdout, "%s%s%s", "\033[1m", text_heard.c_str(), "\033[0m");
                fflush(stdout);

                audio->clear();
            }
        }
	}
}

int main(int argc, char *argv[])
{
	printf("Hello World\n");
	std::thread whisp(whisper_inference_thread);

	whisp.join();

	return 0;
}
