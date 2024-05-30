#ifndef WHISPER_PROCESSING_HPP
#define WHISPER_PROCESSING_HPP
#include <regex>
#include <thread>
#include <stdint.h>
#include <whisper.h>

struct whisper_params {
    int32_t n_threads  = std::min(4, (int32_t) std::thread::hardware_concurrency());
    int32_t voice_ms   = 10000;
    int32_t capture_id = -1;
    int32_t max_tokens = 32;
    int32_t audio_ctx  = 0;
    int32_t n_gpu_layers = 999;

    float vad_thold  = 0.6f;
    float freq_thold = 100.0f;

    bool speed_up       = false;
    bool translate      = false;
    bool print_special  = false;
    bool print_energy   = false;
    bool no_timestamps  = true;
    bool verbose_prompt = false;
    bool use_gpu        = true;
    bool flash_attn     = false;

    std::string person      = "Georgi";
    std::string bot_name    = "LLaMA";
    std::string wake_cmd    = "";
    std::string heard_ok    = "";
    std::string language    = "nl";
    std::string model_wsp   = "../models/ggml-medium.bin";
    std::string model_llama = "../models/llama-2-13b-chat.Q5_0.gguf";
    std::string speak       = "./../speak";
    std::string speak_file  = "./../to_speak";
    std::string prompt      = "";
    std::string fname_out;
    std::string path_session = "";       // path to file for saving/loading model eval state
};

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

std::string post_process_words(const std::vector<std::string> &inp, bool &force_speak) {
	std::string text_heard;

	for (int i = 0; i < (int)inp.size(); ++i)
	{
		text_heard += inp[i] + " ";
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
	
	return text_heard;
}

#endif /* WHISPER_PROCESSING_HPP */