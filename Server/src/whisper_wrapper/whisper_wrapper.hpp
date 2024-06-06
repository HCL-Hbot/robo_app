#ifndef WHISPER_WRAPPER_HPP
#define WHISPER_WRAPPER_HPP
#include <whisper.h>
#include <whisper_helper.hpp>
#include <string>

class whisper_wrapper
{
public:
    whisper_wrapper(whisper_params params);

    void init(bool force_speak = false);

    std::string do_inference(std::vector<float> &pcmf_samples, int64_t &t_ms, bool &no_voice_detected);

    ~whisper_wrapper()
    {
        whisper_print_timings(m_ctx);
        whisper_free(m_ctx);
    }

private:
    whisper_context *m_ctx;
    whisper_params m_params;
    std::vector<float> m_pcmf32_prompt;
    std::string m_prompt_whisper;
    const std::string m_chat_symb = ":";
    float m_prob0 = 0.0f;
    bool m_force_speak;
    struct whisper_context_params m_cparams = whisper_context_default_params();
};
#endif /* WHISPER_WRAPPER_HPP */