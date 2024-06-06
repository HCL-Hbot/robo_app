#pragma once

#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>
#include <memory>
#include <rtp_receiver.hpp>

//
// RTP Audio capture
//

class audio_async {
public:
    audio_async(std::shared_ptr<RtpReceiver> recv, int len_ms);
    ~audio_async();

    bool init();

    // start capturing audio via the provided SDL callback
    // keep last len_ms seconds of audio in a circular buffer
    bool resume();
    bool pause();
    bool clear();

//     // callback to be called by gstreamer

    bool active();
    // get audio data from the circular buffer
    void get(int ms, std::vector<float> & audio);

private:

    int m_len_ms = 0;
    int m_sample_rate = 0;
    void callback(guint8 *data, int len);
    std::atomic_bool m_running;
    std::atomic_bool m_active;
    std::mutex       m_mutex;

    std::shared_ptr<RtpReceiver> _recv_inst;
    std::vector<float> m_audio;
    size_t             m_audio_pos = 0;
    size_t             m_audio_len = 0;
};

