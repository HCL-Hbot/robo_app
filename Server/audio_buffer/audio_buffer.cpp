#include "audio_buffer.hpp"
#include <stdio.h>

audio_async::audio_async(std::shared_ptr<RtpReceiver> recv, int len_ms) {
    m_len_ms = len_ms;

    m_running = false;
    _recv_inst = recv;
}

audio_async::~audio_async() {
//     if (m_dev_id_in) {
//         SDL_CloseAudioDevice(m_dev_id_in);
//     }
// }
}


// callback to be called by SDL
void audio_async::callback(guint8 *data, int len) {
    m_active = true;
    size_t n_samples = len / sizeof(float);

    if (n_samples > m_audio.size()) {
        n_samples = m_audio.size();

        data += (len - (n_samples * sizeof(float)));
    }

    // fprintf(stderr, "%s: %zu samples, pos %zu, len %zu\n", __func__, n_samples, m_audio_pos, m_audio_len);

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_audio_pos + n_samples > m_audio.size()) {
            const size_t n0 = m_audio.size() - m_audio_pos;

            memcpy(&m_audio[m_audio_pos], data, n0 * sizeof(float));
            memcpy(&m_audio[0], data + n0 * sizeof(float), (n_samples - n0) * sizeof(float));

            m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
            m_audio_len = m_audio.size();
        } else {
            memcpy(&m_audio[m_audio_pos], data, n_samples * sizeof(float));

            m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
            m_audio_len = std::min(m_audio_len + n_samples, m_audio.size());
        }
    }
}


bool audio_async::active() {
    return m_active;
}

bool audio_async::init() {

    std::function<void(guint8*, int)> func = std::bind(&audio_async::callback, this, std::placeholders::_1, std::placeholders::_2);
    _recv_inst->start(func); // Assuming start expects a std::function<void(const guint8*, int)>
    m_sample_rate = 16000;

    m_audio.resize((m_sample_rate*m_len_ms)/1000);
    return true;
}

bool audio_async::resume() {

    if (m_running) {
        fprintf(stderr, "%s: already running!\n", __func__);
        return false;
    }
    
    _recv_inst->resume();
    m_running = true;

    return true;
}

bool audio_async::pause() {
    if (!m_running) {
        fprintf(stderr, "%s: already paused!\n", __func__);
        return false;
    }

    _recv_inst->pause();

    m_running = false;

    return true;
}

bool audio_async::clear() {
    if (!m_running) {
        fprintf(stderr, "%s: not running!\n", __func__);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_audio_pos = 0;
        m_audio_len = 0;
    }

    return true;
}



void audio_async::get(int ms, std::vector<float> & result) {
    m_active = false;
    if (!m_running) {
        fprintf(stderr, "%s: not running!\n", __func__);
        return;
    }

    result.clear();

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (ms <= 0) {
            ms = m_len_ms;
        }

        size_t n_samples = (m_sample_rate * ms) / 1000;
        if (n_samples > m_audio_len) {
            n_samples = m_audio_len;
        }

        result.resize(n_samples);

        int s0 = m_audio_pos - n_samples;
        if (s0 < 0) {
            s0 += m_audio.size();
        }

        if (s0 + n_samples > m_audio.size()) {
            const size_t n0 = m_audio.size() - s0;

            memcpy(result.data(), &m_audio[s0], n0 * sizeof(float));
            memcpy(&result[n0], &m_audio[0], (n_samples - n0) * sizeof(float));
        } else {
            memcpy(result.data(), &m_audio[s0], n_samples * sizeof(float));
        }
    }
}