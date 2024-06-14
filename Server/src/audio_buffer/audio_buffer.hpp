#pragma once

#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>
#include <memory>
#include <rtp_receiver.hpp>

/**
 * @class audio_async
 * @brief A class to handle asynchronous audio capture from an RTP stream.
 * 
 * This class captures audio data via an RTP receiver and stores the last `len_ms` milliseconds
 * of audio in a circular buffer.
 */
class audio_async {
public:
    /**
     * @brief Construct a new audio_async object.
     * 
     * @param recv Shared pointer to an RtpReceiver instance.
     * @param len_ms Length of the audio buffer in milliseconds.
     */
    audio_async(std::shared_ptr<RtpReceiver> recv, int len_ms);

    /**
     * @brief Destroy the audio_async object.
     */
    ~audio_async();

    /**
     * @brief Initialize the audio capture.
     * 
     * @return true if initialization was successful, false otherwise.
     */
    bool init();

    /**
     * @brief Start capturing audio.
     * 
     * This method starts capturing audio via the provided SDL callback
     * and keeps the last `len_ms` seconds of audio in a circular buffer.
     * 
     * @return true if resuming was successful, false otherwise.
     */
    bool resume();

    /**
     * @brief Pause audio capturing.
     * 
     * @return true if pausing was successful, false otherwise.
     */
    bool pause();

    /**
     * @brief Clear the audio buffer.
     * 
     * @return true if clearing was successful, false otherwise.
     */
    bool clear();

    /**
     * @brief Check if audio capturing is active.
     * 
     * @return true if audio capturing is active, false otherwise.
     */
    bool active();

    /**
     * @brief Get audio data from the circular buffer.
     * 
     * @param ms The amount of audio data to get in milliseconds.
     * @param audio Vector to store the retrieved audio data.
     */
    void get(int ms, std::vector<float> & audio);

private:
    int m_len_ms = 0;               ///< Length of the audio buffer in milliseconds.
    int m_sample_rate = 0;          ///< Sample rate of the audio.

    /**
     * @brief Callback to be called by the RTP client.
     * 
     * @param data Pointer to the received data.
     * @param len Length of the received data.
     */
    void callback(guint8 *data, int len);

    std::atomic_bool m_running;     ///< Flag indicating if the audio capture is running.
    std::atomic_bool m_active;      ///< Flag indicating if the audio capture is active.
    std::mutex m_mutex;             ///< Mutex for synchronizing access to shared data.

    std::shared_ptr<RtpReceiver> _recv_inst; ///< Shared pointer to the RTP receiver instance.
    std::vector<float> m_audio;     ///< Circular buffer for storing audio data.
    size_t m_audio_pos = 0;         ///< Current position in the audio buffer.
    size_t m_audio_len = 0;         ///< Length of the audio buffer.
};
