#pragma once

#include <alsa/asoundlib.h>

#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>
#include <thread>

class audio_async {
public:
    audio_async(int len_ms);
    ~audio_async();

    bool init(int capture_id, int sample_rate);

    // start capturing audio via the provided SDL callback
    // keep last len_ms seconds of audio in a circular buffer
    bool resume();
    bool pause();
    bool clear();

    // get audio data from the circular buffer
    void get(int ms, std::vector<float> & audio);

    // get audio data size available from the circular buffer
    size_t getSize(int ms);

    void captureLoop();

private:
    snd_pcm_t *captureHandle;
    unsigned int sampleRate;
    snd_pcm_uframes_t bufferSize;
    std::vector<float> buffer;
    std::atomic_bool m_exit;
    std::thread capture;


    int m_len_ms = 0;

    std::atomic_bool m_running;
    std::mutex       m_mutex;

    std::vector<float> m_audio;
    size_t             m_audio_pos = 0;
    size_t             m_audio_len = 0;
};

// Return false if need to quit
bool sdl_poll_events();
