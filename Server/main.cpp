#include <stdio.h>
#include <rtp_receiver.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <whisper_processing.hpp>
#include "common.h"
#include <audio_buffer.hpp>
#include <regex>
#include <llama.h>

std::vector<llama_token> llama_tokenize(struct llama_context * ctx, const std::string & text, bool add_bos) {
    auto * model = llama_get_model(ctx);

    // upper limit for the number of tokens
    int n_tokens = text.length() + add_bos;
    std::vector<llama_token> result(n_tokens);
    n_tokens = llama_tokenize(model, text.data(), text.length(), result.data(), result.size(), add_bos, false);
    if (n_tokens < 0) {
        result.resize(-n_tokens);
        int check = llama_tokenize(model, text.data(), text.length(), result.data(), result.size(), add_bos, false);
        GGML_ASSERT(check == -n_tokens);
    } else {
        result.resize(n_tokens);
    }
    return result;
}

std::string llama_token_to_piece(const struct llama_context * ctx, llama_token token) {
    std::vector<char> result(8, 0);
    const int n_tokens = llama_token_to_piece(llama_get_model(ctx), token, result.data(), result.size(), false);
    if (n_tokens < 0) {
        result.resize(-n_tokens);
        int check = llama_token_to_piece(llama_get_model(ctx), token, result.data(), result.size(), false);
        GGML_ASSERT(check == -n_tokens);
    } else {
        result.resize(n_tokens);
    }

    return std::string(result.data(), result.size());
}

const std::string k_prompt_whisper = R"(A conversation with a person called {1}.)";

const std::string k_prompt_llama = R"(Text transcript of a never ending dialog, where {0} interacts with an AI assistant named {1}.
{1} is helpful, kind, honest, friendly, good at writing and never fails to answer {0}’s requests immediately and with details and precision.
There are no annotations like (30 seconds passed...) or (to himself), just what {0} and {1} say aloud to each other.
The transcript only includes text, it does not include markup like HTML and Markdown.
{1} responds with short and concise answers.

{0}{4} Hello, {1}!
{1}{4} Hello {0}! How may I help you today?
{0}{4} What time is it?
{1}{4} It is {2} o'clock.
{0}{4} What year is it?
{1}{4} We are in {3}.
{0}{4} What is a cat?
{1}{4} A cat is a domestic species of small carnivorous mammal. It is the only domesticated species in the family Felidae.
{0}{4} Name a color.
{1}{4} Blue
{0}{4})";

int whisper_inference_thread()
{
	int port = 5004;
	std::shared_ptr<RtpReceiver> Receiver = std::make_shared<RtpReceiver>(port);
	std::shared_ptr<audio_async> audio = std::make_shared<audio_async>(Receiver, 30 * 1000);
	audio->init();
	whisper_params params;
	
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
	/* Whisper variables */	
	
	float prob0 = 0.0f;
	bool force_speak = false;
	std::vector<float> buffer;
	int threshold = 50;

    // llama init

    llama_backend_init();

    auto lmparams = llama_model_default_params();
    if (!params.use_gpu) {
        lmparams.n_gpu_layers = 0;
    } else {
        lmparams.n_gpu_layers = params.n_gpu_layers;
    }

    struct llama_model * model_llama = llama_load_model_from_file(params.model_llama.c_str(), lmparams);
    if (!model_llama) {
        fprintf(stderr, "No llama.cpp model specified. Please provide using -ml <modelfile>\n");
        return 1;
    }

    llama_context_params lcparams = llama_context_default_params();

    // tune these to your liking
    lcparams.n_ctx      = 2048;
    lcparams.seed       = 1;
    lcparams.n_threads  = params.n_threads;
    lcparams.flash_attn = params.flash_attn;

    struct llama_context * ctx_llama = llama_new_context_with_model(model_llama, lcparams);

    // print some info about the processing
    {
        fprintf(stderr, "\n");

        if (!whisper_is_multilingual(ctx_wsp)) {
            if (params.language != "en" || params.translate) {
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


    const std::string chat_symb = ":";

    std::vector<float> pcmf32_prompt;

    const std::string prompt_whisper = ::replace(k_prompt_whisper, "{1}", params.bot_name);

	    // construct the initial prompt for LLaMA inference
    std::string prompt_llama = params.prompt.empty() ? k_prompt_llama : params.prompt;

    // need to have leading ' '
    prompt_llama.insert(0, 1, ' ');

    prompt_llama = ::replace(prompt_llama, "{0}", params.person);
    prompt_llama = ::replace(prompt_llama, "{1}", params.bot_name);

    {
        // get time string
        std::string time_str;
        {
            time_t t = time(0);
            struct tm * now = localtime(&t);
            char buf[128];
            strftime(buf, sizeof(buf), "%H:%M", now);
            time_str = buf;
        }
        prompt_llama = ::replace(prompt_llama, "{2}", time_str);
    }

    {
        // get year string
        std::string year_str;
        {
            time_t t = time(0);
            struct tm * now = localtime(&t);
            char buf[128];
            strftime(buf, sizeof(buf), "%Y", now);
            year_str = buf;
        }
        prompt_llama = ::replace(prompt_llama, "{3}", year_str);
    }

    prompt_llama = ::replace(prompt_llama, "{4}", chat_symb);

    llama_batch batch = llama_batch_init(llama_n_ctx(ctx_llama), 0, 1);

    // init session
    std::string path_session = params.path_session;
    std::vector<llama_token> session_tokens;

	auto embd_inp = ::llama_tokenize(ctx_llama, prompt_llama, true);

    if (!path_session.empty()) {
        fprintf(stderr, "%s: attempting to load saved session from %s\n", __func__, path_session.c_str());

        // fopen to check for existing session
        FILE * fp = std::fopen(path_session.c_str(), "rb");
        if (fp != NULL) {
            std::fclose(fp);

            session_tokens.resize(llama_n_ctx(ctx_llama));
            size_t n_token_count_out = 0;
            if (!llama_load_session_file(ctx_llama, path_session.c_str(), session_tokens.data(), session_tokens.capacity(), &n_token_count_out)) {
                fprintf(stderr, "%s: error: failed to load session file '%s'\n", __func__, path_session.c_str());
                return 1;
            }
            session_tokens.resize(n_token_count_out);
            for (size_t i = 0; i < session_tokens.size(); i++) {
                embd_inp[i] = session_tokens[i];
            }

            fprintf(stderr, "%s: loaded a session with prompt size of %d tokens\n", __func__, (int) session_tokens.size());
        } else {
            fprintf(stderr, "%s: session file does not exist, will create\n", __func__);
        }
    }

    // evaluate the initial prompt

    printf("\n");
    printf("%s : initializing - please wait ...\n", __func__);

    // // prepare batch
    // {
    //     batch.n_tokens = embd_inp.size();

    //     for (int i = 0; i < batch.n_tokens; i++) {
    //         batch.token[i]     = embd_inp[i];
    //         batch.pos[i]       = i;
    //         batch.n_seq_id[i]  = 1;
    //         batch.seq_id[i][0] = 0;
    //         batch.logits[i]    = i == batch.n_tokens - 1;
    //     }
    // }

    // if (llama_decode(ctx_llama, batch)) {
    //     fprintf(stderr, "%s : failed to decode\n", __func__);
    //     return 1;
    // }

    if (params.verbose_prompt) {
        fprintf(stdout, "\n");
        fprintf(stdout, "%s", prompt_llama.c_str());
        fflush(stdout);
    }

     // debug message about similarity of saved session, if applicable
    size_t n_matching_session_tokens = 0;
    if (session_tokens.size()) {
        for (llama_token id : session_tokens) {
            if (n_matching_session_tokens >= embd_inp.size() || id != embd_inp[n_matching_session_tokens]) {
                break;
            }
            n_matching_session_tokens++;
        }
        if (n_matching_session_tokens >= embd_inp.size()) {
            fprintf(stderr, "%s: session file has exact match for prompt!\n", __func__);
        } else if (n_matching_session_tokens < (embd_inp.size() / 2)) {
            fprintf(stderr, "%s: warning: session file has low similarity to prompt (%zu / %zu tokens); will mostly be reevaluated\n",
                __func__, n_matching_session_tokens, embd_inp.size());
        } else {
            fprintf(stderr, "%s: session file matches %zu / %zu tokens of prompt\n",
                __func__, n_matching_session_tokens, embd_inp.size());
        }
    }

    bool need_to_save_session = !path_session.empty() && n_matching_session_tokens < (embd_inp.size() * 3 / 4);


    printf("\n");
    printf("%s%s", params.person.c_str(), chat_symb.c_str());
    fflush(stdout);

    // clear audio buffer
    audio->clear();

	/* LLama variables */

    // text inference variables
    const int voice_id = 2;
    const int n_keep   = embd_inp.size();
    const int n_ctx    = llama_n_ctx(ctx_llama);

    int n_past = n_keep;
    int n_prev = 64; // TODO arg
    int n_session_consumed = !path_session.empty() && session_tokens.size() > 0 ? session_tokens.size() : 0;

    std::vector<llama_token> embd;

    // reverse prompts for detecting when it's time to stop speaking
    std::vector<std::string> antiprompts = {
        params.person + chat_symb,
    };




	audio->resume();
	while (1)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

        int64_t t_ms = 0;
        {
            audio->get(2000, buffer);
            if (::vad_simple(buffer, WHISPER_SAMPLE_RATE, 1250, params.vad_thold, params.freq_thold, params.print_energy) || force_speak) {
                fprintf(stdout, "%s: Speech detected! Processing ...\n", __func__);

                audio->get(params.voice_ms, buffer);

                std::string all_heard;

                if (!force_speak) {
                    all_heard = ::trim(::transcribe(ctx_wsp, params, buffer, prob0, t_ms));
                }

                const auto words = get_words(all_heard);

				std::string text_heard = post_process_words(words, force_speak);
				// if(text_heard.empty()) {
				// 	audio->clear();
				// 	continue;
				// }
				// text_heard.append(" geef kort antwoord zonder emoticons in het Nederlands!");

   				 const std::vector<llama_token> tokens = llama_tokenize(ctx_llama, text_heard.c_str(), false);

                if (text_heard.empty() || tokens.empty() || force_speak) {
                    //fprintf(stdout, "%s: Heard nothing, skipping ...\n", __func__);
                    audio->clear();

                    continue;
                }
                embd = ::llama_tokenize(ctx_llama, text_heard, false);

                // Append the new input tokens to the session_tokens vector
                if (!path_session.empty()) {
                    session_tokens.insert(session_tokens.end(), tokens.begin(), tokens.end());
                }

                // text inference
                bool done = false;
                std::string text_to_speak;
                while (true) {
                    // predict
                    if (embd.size() > 0) {
                        if (n_past + (int) embd.size() > n_ctx) {
                            n_past = n_keep;

                            // insert n_left/2 tokens at the start of embd from last_n_tokens
                            embd.insert(embd.begin(), embd_inp.begin() + embd_inp.size() - n_prev, embd_inp.end());
                            // stop saving session if we run out of context
                            path_session = "";
                            //printf("\n---\n");
                            //printf("resetting: '");
                            //for (int i = 0; i < (int) embd.size(); i++) {
                            //    printf("%s", llama_token_to_piece(ctx_llama, embd[i]));
                            //}
                            //printf("'\n");
                            //printf("\n---\n");
                        }

                        // try to reuse a matching prefix from the loaded session instead of re-eval (via n_past)
                        // REVIEW
                        if (n_session_consumed < (int) session_tokens.size()) {
                            size_t i = 0;
                            for ( ; i < embd.size(); i++) {
                                if (embd[i] != session_tokens[n_session_consumed]) {
                                    session_tokens.resize(n_session_consumed);
                                    break;
                                }

                                n_past++;
                                n_session_consumed++;

                                if (n_session_consumed >= (int) session_tokens.size()) {
                                    i++;
                                    break;
                                }
                            }
                            if (i > 0) {
                                embd.erase(embd.begin(), embd.begin() + i);
                            }
                        }

                        if (embd.size() > 0 && !path_session.empty()) {
                            session_tokens.insert(session_tokens.end(), embd.begin(), embd.end());
                            n_session_consumed = session_tokens.size();
                        }

                        // prepare batch
                        {
                            batch.n_tokens = embd.size();

                            for (int i = 0; i < batch.n_tokens; i++) {
                                batch.token[i]     = embd[i];
                                batch.pos[i]       = n_past + i;
                                batch.n_seq_id[i]  = 1;
                                batch.seq_id[i][0] = 0;
                                batch.logits[i]    = i == batch.n_tokens - 1;
                            }
                        }

                        if (llama_decode(ctx_llama, batch)) {
                            fprintf(stderr, "%s : failed to decode\n", __func__);
                            return 1;
                        }
                    }


                    embd_inp.insert(embd_inp.end(), embd.begin(), embd.end());
                    n_past += embd.size();

                    embd.clear();

                    if (done) break;

                    {
                        // out of user input, sample next token
                        const float top_k          = 5;
                        const float top_p          = 0.80f;
                        const float temp           = 0.30f;
                        const float repeat_penalty = 1.1764f;

                        const int repeat_last_n    = 256;

                        llama_token id = 0;

                        {
                            auto logits = llama_get_logits(ctx_llama);
                            auto n_vocab = llama_n_vocab(model_llama);

                            logits[llama_token_eos(model_llama)] = 0;

                            std::vector<llama_token_data> candidates;
                            candidates.reserve(n_vocab);
                            for (llama_token token_id = 0; token_id < n_vocab; token_id++) {
                                candidates.emplace_back(llama_token_data{token_id, logits[token_id], 0.0f});
                            }

                            llama_token_data_array candidates_p = { candidates.data(), candidates.size(), false };

                            // apply repeat penalty
                            const float nl_logit = logits[llama_token_nl(model_llama)];

                            llama_sample_repetition_penalties(ctx_llama, &candidates_p,
                                    embd_inp.data() + std::max(0, n_past - repeat_last_n),
                                    repeat_last_n, repeat_penalty, 0.0, 0.0f);

                            logits[llama_token_nl(model_llama)] = nl_logit;

                            if (temp <= 0) {
                                // Greedy sampling
                                id = llama_sample_token_greedy(ctx_llama, &candidates_p);
                            } else {
                                // Temperature sampling
                                llama_sample_top_k(ctx_llama, &candidates_p, top_k, 1);
                                llama_sample_top_p(ctx_llama, &candidates_p, top_p, 1);
                                llama_sample_temp (ctx_llama, &candidates_p, temp);
                                id = llama_sample_token(ctx_llama, &candidates_p);
                            }
                        }

                        if (id != llama_token_eos(model_llama)) {
                            // add it to the context
                            embd.push_back(id);

                            text_to_speak += llama_token_to_piece(ctx_llama, id);

                            printf("%s", llama_token_to_piece(ctx_llama, id).c_str());
                            fflush(stdout);
                        }
                    }

                    {
                        std::string last_output;
                        for (int i = embd_inp.size() - 16; i < (int) embd_inp.size(); i++) {
                            last_output += llama_token_to_piece(ctx_llama, embd_inp[i]);
                        }
                        last_output += llama_token_to_piece(ctx_llama, embd[0]);

                        for (std::string & antiprompt : antiprompts) {
                            if (last_output.find(antiprompt.c_str(), last_output.length() - antiprompt.length(), antiprompt.length()) != std::string::npos) {
                                done = true;
                                text_to_speak = ::replace(text_to_speak, antiprompt, "");
                                fflush(stdout);
                                need_to_save_session = true;
                                break;
                            }
                        }
                    }
                }

                speak_with_file(params.speak, text_to_speak, params.speak_file, voice_id);
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
