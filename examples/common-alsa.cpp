#include "common-alsa.h"

#include <iostream>
#include <alsa/asoundlib.h>


audio_async::audio_async(int len_ms) {
    m_len_ms = len_ms;

    m_running = false;
    m_exit = false;
}

audio_async::~audio_async() {
    // if (m_dev_id_in) {
    //     SDL_CloseAudioDevice(m_dev_id_in);
    // }

    // make thread exit
    m_exit = true;

    if (captureHandle) {
        snd_pcm_close(captureHandle);
    }
}

bool audio_async::init(int capture_id, int sample_rate) {
    // SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    // if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    //     SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
    //     return false;
    // }

    // SDL_SetHintWithPriority(SDL_HINT_AUDIO_RESAMPLING_MODE, "medium", SDL_HINT_OVERRIDE);

    // {
    //     int nDevices = SDL_GetNumAudioDevices(SDL_TRUE);
    //     fprintf(stderr, "%s: found %d capture devices:\n", __func__, nDevices);
    //     for (int i = 0; i < nDevices; i++) {
    //         fprintf(stderr, "%s:    - Capture device #%d: '%s'\n", __func__, i, SDL_GetAudioDeviceName(i, SDL_TRUE));
    //     }
    // }

    // SDL_AudioSpec capture_spec_requested;
    // SDL_AudioSpec capture_spec_obtained;

    // SDL_zero(capture_spec_requested);
    // SDL_zero(capture_spec_obtained);

    // capture_spec_requested.freq     = sample_rate;
    // capture_spec_requested.format   = AUDIO_F32;
    // capture_spec_requested.channels = 1;
    // capture_spec_requested.samples  = 1024;
    // capture_spec_requested.callback = [](void * userdata, uint8_t * stream, int len) {
    //     audio_async * audio = (audio_async *) userdata;
    //     audio->callback(stream, len);
    // };
    // capture_spec_requested.userdata = this;

    // if (capture_id >= 0) {
    //     fprintf(stderr, "%s: attempt to open capture device %d : '%s' ...\n", __func__, capture_id, SDL_GetAudioDeviceName(capture_id, SDL_TRUE));
    //     m_dev_id_in = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(capture_id, SDL_TRUE), SDL_TRUE, &capture_spec_requested, &capture_spec_obtained, 0);
    // } else {
    //     fprintf(stderr, "%s: attempt to open default capture device ...\n", __func__);
    //     m_dev_id_in = SDL_OpenAudioDevice(nullptr, SDL_TRUE, &capture_spec_requested, &capture_spec_obtained, 0);
    // }

    // if (!m_dev_id_in) {
    //     fprintf(stderr, "%s: couldn't open an audio device for capture: %s!\n", __func__, SDL_GetError());
    //     m_dev_id_in = 0;

    //     return false;
    // } else {
    //     fprintf(stderr, "%s: obtained spec for input device (SDL Id = %d):\n", __func__, m_dev_id_in);
    //     fprintf(stderr, "%s:     - sample rate:       %d\n",                   __func__, capture_spec_obtained.freq);
    //     fprintf(stderr, "%s:     - format:            %d (required: %d)\n",    __func__, capture_spec_obtained.format,
    //             capture_spec_requested.format);
    //     fprintf(stderr, "%s:     - channels:          %d (required: %d)\n",    __func__, capture_spec_obtained.channels,
    //             capture_spec_requested.channels);
    //     fprintf(stderr, "%s:     - samples per frame: %d\n",                   __func__, capture_spec_obtained.samples);
    // }

    // m_sample_rate = capture_spec_obtained.freq;

    // m_audio.resize((m_sample_rate*m_len_ms)/1000);

    captureHandle = nullptr;
    sampleRate = 16000;         // 44100;
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

    // try {
    //     // std::thread captureThread(&audio_async::captureLoop, this);
    //     std::thread captureThread([this] { this->captureLoop(); } );
        
    //     // Perform other tasks or processing here...

    //     // captureThread.join();

    // } catch (const std::exception &e) {
    //     std::cerr << "Exception: " << e.what() << std::endl;
    //     return 1;
    // }

    return true;
}

bool audio_async::resume() {
    // if (!m_dev_id_in) {
    //     fprintf(stderr, "%s: no audio device to resume!\n", __func__);
    //     return false;
    // }

    // if (m_running) {
    //     fprintf(stderr, "%s: already running!\n", __func__);
    //     return false;
    // }

    // SDL_PauseAudioDevice(m_dev_id_in, 0);

    // m_running = true;

    // return true;

    if (!captureHandle) {
        fprintf(stderr, "%s: no audio device to resume!\n", __func__);
        return false;
    }

    if (m_running) {
        fprintf(stderr, "%s: already running!\n", __func__);
        return false;
    }

    // SDL_PauseAudioDevice(m_dev_id_in, 0);
    snd_pcm_pause(captureHandle, 0);

    m_running = true;

    return true;

}

bool audio_async::pause() {
    // if (!m_dev_id_in) {
    //     fprintf(stderr, "%s: no audio device to pause!\n", __func__);
    //     return false;
    // }

    // if (!m_running) {
    //     fprintf(stderr, "%s: already paused!\n", __func__);
    //     return false;
    // }

    // SDL_PauseAudioDevice(m_dev_id_in, 1);

    // m_running = false;

    // return true;

    if (!captureHandle) {
        fprintf(stderr, "%s: no audio device to pause!\n", __func__);
        return false;
    }

    if (!m_running) {
        fprintf(stderr, "%s: already paused!\n", __func__);
        return false;
    }

    // SDL_PauseAudioDevice(m_dev_id_in, 1);
    snd_pcm_pause(captureHandle, 1);

    m_running = false;

    return true;
}

bool audio_async::clear() {
    // if (!m_dev_id_in) {
    //     fprintf(stderr, "%s: no audio device to clear!\n", __func__);
    //     return false;
    // }

    // if (!m_running) {
    //     fprintf(stderr, "%s: not running!\n", __func__);
    //     return false;
    // }

    // {
    //     std::lock_guard<std::mutex> lock(m_mutex);

    //     m_audio_pos = 0;
    //     m_audio_len = 0;
    // }

    // return true;

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

// // callback to be called by SDL
// void audio_async::callback(uint8_t * stream, int len) {
//     if (!m_running) {
//         return;
//     }

//     const size_t n_samples = len / sizeof(float);

//     m_audio_new.resize(n_samples);
//     memcpy(m_audio_new.data(), stream, n_samples * sizeof(float));

//     //fprintf(stderr, "%s: %zu samples, pos %zu, len %zu\n", __func__, n_samples, m_audio_pos, m_audio_len);

//     {
//         std::lock_guard<std::mutex> lock(m_mutex);

//         if (m_audio_pos + n_samples > m_audio.size()) {
//             const size_t n0 = m_audio.size() - m_audio_pos;

//             memcpy(&m_audio[m_audio_pos], stream, n0 * sizeof(float));
//             memcpy(&m_audio[0], &stream[n0], (n_samples - n0) * sizeof(float));

//             m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
//             m_audio_len = m_audio.size();
//         } else {
//             memcpy(&m_audio[m_audio_pos], stream, n_samples * sizeof(float));

//             m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
//             m_audio_len = std::min(m_audio_len + n_samples, m_audio.size());
//         }
//     }
// }

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

        m_audio_new.resize(n_samples);
        memcpy(m_audio_new.data(), buffer.data(), n_samples * sizeof(float));

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

        // std::cout << buffer.at(0) << std::endl;
    }
}

void audio_async::get(int ms, std::vector<float> & result) {
    // if (!m_dev_id_in) {
    //     fprintf(stderr, "%s: no audio device to get audio from!\n", __func__);
    //     return;
    // }

    // if (!m_running) {
    //     fprintf(stderr, "%s: not running!\n", __func__);
    //     return;
    // }

    // result.clear();

    // {
    //     std::lock_guard<std::mutex> lock(m_mutex);

    //     if (ms <= 0) {
    //         ms = m_len_ms;
    //     }

    //     size_t n_samples = (m_sample_rate * ms) / 1000;
    //     if (n_samples > m_audio_len) {
    //         n_samples = m_audio_len;
    //     }

    //     result.resize(n_samples);

    //     int s0 = m_audio_pos - n_samples;
    //     if (s0 < 0) {
    //         s0 += m_audio.size();
    //     }

    //     if (s0 + n_samples > m_audio.size()) {
    //         const size_t n0 = m_audio.size() - s0;

    //         memcpy(result.data(), &m_audio[s0], n0 * sizeof(float));
    //         memcpy(&result[n0], &m_audio[0], (n_samples - n0) * sizeof(float));
    //     } else {
    //         memcpy(result.data(), &m_audio[s0], n_samples * sizeof(float));
    //     }
    // }

    // std::cout << "audio_async::get() called" << std::endl;

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
    // SDL_Event event;
    // while (SDL_PollEvent(&event)) {
    //     switch (event.type) {
    //         case SDL_QUIT:
    //             {
    //                 return false;
    //             } break;
    //         default:
    //             break;
    //     }
    // }

    return true;
}
