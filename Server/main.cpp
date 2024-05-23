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
#include "common-sdl.h"
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
	std::string model_wsp = "ggml-medium.bin";
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

class Pcm32fBuffer {
public:
    // Constructor: Initialize buffer with a size for 30 seconds of 16 kHz PCM32F audio
    Pcm32fBuffer()
        : buffer(16 * 1000 * 30), writeIndex(0), readIndex(0), updated(false) {}

    // Update the buffer with new data from a C-style array
    void updateBuffer(const float *newBuffer, std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex);

        // Ensure we don't write more data than the buffer can hold at once
        size = std::min(size, buffer.size());

        for (std::size_t i = 0; i < size; ++i) {
            buffer[writeIndex] = newBuffer[i];
            writeIndex = (writeIndex + 1) % buffer.size();

            if (writeIndex == readIndex) {
                // Buffer overflow, move readIndex forward
                readIndex = (readIndex + 1) % buffer.size();
            }
        }
        updated = true;
        updateCondition.notify_all();
    }

    // Read the buffer into an output vector for a specified duration in milliseconds
    std::vector<float> readBuffer(std::size_t duration_ms) {
        std::lock_guard<std::mutex> lock(mutex);

        // Calculate the number of samples to read
        std::size_t numSamples = duration_ms * 16; // 16 samples per millisecond
        std::vector<float> outputBuffer;
        numSamples = std::min(numSamples, buffer.size());

        for (std::size_t i = 0; i < numSamples; ++i) {
            outputBuffer.push_back(buffer[readIndex]);
            readIndex = (readIndex + 1) % buffer.size();
        }

        updated = false; // Reset the updated flag when read
        return outputBuffer;
    }

    // Check if the buffer has been updated
    bool isUpdated() const {
        return updated.load();
    }

    // Wait until the buffer is updated
    void waitForUpdate() {
        std::unique_lock<std::mutex> lock(mutex);
        updateCondition.wait(lock, [this] { return updated.load(); });
    }

private:
    std::vector<float> buffer;
    std::size_t writeIndex;
    std::size_t readIndex;
    mutable std::mutex mutex;
    std::condition_variable updateCondition;
    std::atomic<bool> updated;
};
Pcm32fBuffer pcmBuffer;

void rtp_client_thread()
{
	int port = 5004;
	RtpReceiver receiver(port);

	auto callback = [](const float *data, size_t size)
	{
		// Process the PCM 32-bit float samples here
		pcmBuffer.updateBuffer(data, (size / sizeof(float)));
	};

	receiver.start(callback);
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

std::vector<float> downsample(const std::vector<float> &inputBuffer)
{
	std::size_t inputSize = inputBuffer.size();
	std::size_t outputSize = inputSize / 3;
	std::vector<float> outputBuffer(outputSize);

	for (std::size_t i = 0; i < outputSize; ++i)
	{
		outputBuffer[i] = inputBuffer[i * 3];
	}

	return outputBuffer;
}

void whisper_inference_thread()
{
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
	int64_t t_ms = 0;
	int i = 0;
	std::vector<float> buffer;
	int threshold = 50;
	while (1)
	{
		pcmBuffer.waitForUpdate();
		buffer = pcmBuffer.readBuffer(2000);
			if (::vad_simple(buffer, WHISPER_SAMPLE_RATE, 1250, params.vad_thold, params.freq_thold, params.print_energy) || force_speak)
			{
				printf("speech detected!");
				buffer = pcmBuffer.readBuffer(params.voice_ms);

                std::string text_heard;

                if (!force_speak) {
                    text_heard = ::trim(::transcribe(ctx_wsp, params, buffer, prob0, t_ms));
                }
				std::cout << text_heard << '\n';
		
			}
			usleep(100*1000);
			// std::cout << buffer.size() << '\n';
			// buffer.clear();
			// i = 0;
		//  writePCMToFile("test.pcm", buffer.data(), buffer.size(), true);
	}
}

int main(int argc, char *argv[])
{
	printf("Hello World\n");
	std::thread rtpcli(rtp_client_thread);
	std::thread whisp(whisper_inference_thread);

	rtpcli.join();
	whisp.join();

	return 0;
}
