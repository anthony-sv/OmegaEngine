module;

// miniaudio is a single-header library: exactly ONE translation unit
// defines the implementation (same drill as stb).
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

module Engine.Audio:AudioEngine;

import :AudioEngine;
import std;

namespace Engine::Audio
{

    struct AudioEngine::Impl
    {
        ma_engine engine {};
        bool      ready  { false };

        // The single music track. ma_sound has no safe default state, so a
        // flag tracks whether it currently wraps a live stream.
        ma_sound  music       {};
        bool      musicLive   { false };
        float     musicVolume { 1.0f };
	}; // struct AudioEngine::Impl

    AudioEngine& AudioEngine::instance()
    {
        static AudioEngine s_instance;
        return s_instance;
    }

    AudioEngine::AudioEngine()
        : m_impl { std::make_unique<Impl>() }
    {
        if (ma_engine_init(nullptr, &m_impl->engine) == MA_SUCCESS)
        {
            m_impl->ready = true;
            std::println("[Ω::Audio] device ready ({} Hz)",
                         ma_engine_get_sample_rate(&m_impl->engine));
        }
        else
        {
            std::println(std::cerr, "[Ω::Audio] device init FAILED — audio disabled");
        }
    }

    AudioEngine::~AudioEngine()
    {
        if (!m_impl)
            return;
        stopMusic();
        if (m_impl->ready)
            ma_engine_uninit(&m_impl->engine);
    }

    bool AudioEngine::ready() const
    {
        return m_impl->ready;
    }

    void AudioEngine::playSound(std::string const& path)
    {
        if (!m_impl->ready)
            return;
        if (ma_engine_play_sound(&m_impl->engine, path.c_str(), nullptr) != MA_SUCCESS)
            std::println(std::cerr, "[Ω::Audio] can't play '{}'", path);
    }

    void AudioEngine::playMusic(std::string const& path, bool loop)
    {
        if (!m_impl->ready)
            return;
        stopMusic();

        // STREAM: decoded on the fly instead of loaded whole -- music tracks
        // are long; effects (above) are small enough to just load.
        if (ma_sound_init_from_file(&m_impl->engine, path.c_str(),
                                    MA_SOUND_FLAG_STREAM, nullptr, nullptr,
                                    &m_impl->music) != MA_SUCCESS)
        {
            std::println(std::cerr, "[Ω::Audio] can't stream '{}'", path);
            return;
        }
        m_impl->musicLive = true;
        ma_sound_set_looping(&m_impl->music, loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&m_impl->music, m_impl->musicVolume);
        ma_sound_start(&m_impl->music);
    }

    void AudioEngine::stopMusic()
    {
        if (m_impl->musicLive)
        {
            ma_sound_uninit(&m_impl->music);
            m_impl->musicLive = false;
        }
    }

    void AudioEngine::setMasterVolume(float volume)
    {
        if (m_impl->ready)
            ma_engine_set_volume(&m_impl->engine, volume);
    }

    void AudioEngine::setMusicVolume(float volume)
    {
        m_impl->musicVolume = volume;
        if (m_impl->musicLive)
            ma_sound_set_volume(&m_impl->music, volume);
    }

} // namespace Audio