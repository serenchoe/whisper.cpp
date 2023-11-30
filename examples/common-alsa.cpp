#include "common-alsa.h"

#include <iostream>
#include <alsa/asoundlib.h>


audio_async::audio_async(int len_ms) {
    m_len_ms = len_ms;

    m_running = false;
    m_exit = false;
}

audio_async::~audio_async() {
    // make thread exit
    m_exit = true;

    if (captureHandle) {
        snd_pcm_close(captureHandle);
    }
}

bool audio_async::init(int capture_id, int sample_rate) {

    captureHandle = nullptr;
    sampleRate = 16000;
    bufferSize = 4096; 

    int err = snd_pcm_open(&captureHandle, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        std::cerr << "Cannot open audio device: " << snd_strerror(err) << std::endl;
        throw std::runtime_error("Failed to open audio device");
    }

    snd_pcm_hw_params_t *hwParams;
    snd_pcm_hw_params_malloc(&hwParams);
    snd_pcm_hw_params_any(captureHandle, hwParams);
    snd_pcm_hw_params_set_access(captureHandle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(captureHandle, hwParams, SND_PCM_FORMAT_FLOAT_LE);         // SND_PCM_FORMAT_S16_LE           
    snd_pcm_hw_params_set_channels(captureHandle, hwParams, 1);
    snd_pcm_hw_params_set_rate_near(captureHandle, hwParams, &sampleRate, nullptr);
    snd_pcm_hw_params_set_period_size_near(captureHandle, hwParams, &bufferSize, nullptr);

    err = snd_pcm_hw_params(captureHandle, hwParams);
    if (err < 0) {
        std::cerr << "Cannot set HW parameters: " << snd_strerror(err) << std::endl;
        throw std::runtime_error("Failed to set HW parameters");
    }
    else {
        std::cout << "sampleRate: " << sampleRate << std::endl;
        std::cout << "bufferSize: " << bufferSize << std::endl;
        std::cout << "m_len_ms: " << m_len_ms << std::endl;
    }

    snd_pcm_hw_params_free(hwParams);

    buffer.resize(bufferSize);

    m_audio.resize((sampleRate*m_len_ms)/1000);

    std::cout << "start captureThread" << std::endl;

    try {
        capture = std::thread( [this] { this->captureLoop(); } );
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return true;
}

bool audio_async::resume() {

    if (!captureHandle) {
        fprintf(stderr, "%s: no audio device to resume!\n", __func__);
        return false;
    }

    if (m_running) {
        fprintf(stderr, "%s: already running!\n", __func__);
        return false;
    }

    snd_pcm_pause(captureHandle, 0);

    m_running = true;

    return true;

}

bool audio_async::pause() {

    if (!captureHandle) {
        fprintf(stderr, "%s: no audio device to pause!\n", __func__);
        return false;
    }

    if (!m_running) {
        fprintf(stderr, "%s: already paused!\n", __func__);
        return false;
    }

    snd_pcm_pause(captureHandle, 1);

    m_running = false;

    return true;
}

bool audio_async::clear() {

    if (!captureHandle) {
        fprintf(stderr, "%s: no audio device to clear!\n", __func__);
        return false;
    }

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


void audio_async::captureLoop() {
    uint8_t *stream;

    std::cout << "captureLoop() started"  << std::endl;

    while (true) {

        if (m_exit)
            break;

        int readBufferSize = snd_pcm_readi(captureHandle, buffer.data(), bufferSize);
        if (readBufferSize < 0) {
            std::cerr << "Capture error: " << snd_strerror(readBufferSize) << std::endl;
        }

        if (readBufferSize < bufferSize)
            std::cout << "readBufferSize: " << readBufferSize  << std::endl;

        // Process captured audio in buffer here...
        if (!m_running) {
            continue;
        }

        stream = (uint8_t*) buffer.data();
        const size_t n_samples = readBufferSize;

        // m_audio_new.resize(n_samples);
        // memcpy(m_audio_new.data(), buffer.data(), n_samples * sizeof(float));

        //fprintf(stderr, "%s: %zu samples, pos %zu, len %zu\n", __func__, n_samples, m_audio_pos, m_audio_len);

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // std::cout << "m_audio.size(): " << m_audio.size() << std::endl;
            // std::cout << "m_audio_pos: " << m_audio_pos << std::endl;
            // std::cout << "n_samples: " << n_samples  << std::endl;

            if (m_audio_pos + n_samples > m_audio.size()) {
                const size_t n0 = m_audio.size() - m_audio_pos;

                memcpy(&m_audio[m_audio_pos], stream, n0 * sizeof(float));
                memcpy(&m_audio[0], &stream[n0], (n_samples - n0) * sizeof(float));

                m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
                m_audio_len = m_audio.size();
            } else {
                memcpy(&m_audio[m_audio_pos], stream, n_samples * sizeof(float));

                m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
                m_audio_len = std::min(m_audio_len + n_samples, m_audio.size());
            }
        }

    }
}

void audio_async::get(int ms, std::vector<float> & result) {

    if (!captureHandle) {
        fprintf(stderr, "%s: no audio device to get audio from!\n", __func__);
        return;
    }

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

        size_t n_samples = (sampleRate * ms) / 1000;    // samples to read from m_audio vector
        if (n_samples > m_audio_len) {                  // if sampes to read is larger than samples available from m_audio vector
            n_samples = m_audio_len;
        }

        result.resize(n_samples);                       // resize result vector where read samples will be stored

        int s0 = m_audio_pos - n_samples;               // start index from which samples will be read
        if (s0 < 0) {
            s0 += m_audio.size();
        }

        if (s0 + n_samples > m_audio.size()) {          // if wrap around happens while reading
            const size_t n0 = m_audio.size() - s0;

            memcpy(result.data(), &m_audio[s0], n0 * sizeof(float));
            memcpy(&result[n0], &m_audio[0], (n_samples - n0) * sizeof(float));
        } else {
            memcpy(result.data(), &m_audio[s0], n_samples * sizeof(float));
        }
    }
}

bool sdl_poll_events() {

    return true;
}
