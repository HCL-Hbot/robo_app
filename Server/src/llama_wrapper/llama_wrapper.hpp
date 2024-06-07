#ifndef LLAMA_WRAPPER_HPP
#define LLAMA_WRAPPER_HPP
#include <whisper_helper.hpp>
#include <llama.h>
#include <common.h>
#include "llama_helper.hpp"
#include <string>
#include <settings.hpp>

class llama_wrapper
{
public:
    llama_wrapper(model_params params);

    void init();

    std::string do_inference(std::string text_heard);

    ~llama_wrapper()
    {
        llama_print_timings(m_ctx);
        llama_free(m_ctx);
    }

private:
    llama_model *model_llama;
    llama_context *m_ctx;

    llama_context_params m_lcparams = llama_context_default_params();
    llama_model_params m_lmparams = llama_model_default_params();
    model_params m_params;

    std::vector<llama_token> m_session_tokens;
    std::vector<std::string> m_antiprompts;
    std::vector<llama_token> m_embd;
    std::vector<gpt_vocab::id> m_embd_inp;

    llama_batch m_batch;

    std::string m_path_session;
    bool m_need_to_save_session;
    int m_n_past;
    int m_n_prev;
    int m_n_session_consumed;
    const int m_voice_id = 2;
    int m_n_keep;
    int m_n_ctx;
    bool m_is_running;


    void prepare_batch(std::vector<llama_token> &tokens, int n_past){
        {
            m_batch.n_tokens = tokens.size();

            for (int i = 0; i < m_batch.n_tokens; i++)
            {
                m_batch.token[i] = tokens[i];
                m_batch.pos[i] = n_past + i;
                m_batch.n_seq_id[i] = 1;
                m_batch.seq_id[i][0] = 0;
                m_batch.logits[i] = i == m_batch.n_tokens - 1;
            }
        }
    }
};

#endif /* LLAMA_WRAPPER_HPP */