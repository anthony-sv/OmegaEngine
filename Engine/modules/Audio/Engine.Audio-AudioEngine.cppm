export module Engine.Audio:AudioEngine;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: AudioEngine
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Audio
{

    // =================================================================
    //
    //  AudioEngine -- the miniaudio wrapper (PIMPL singleton).
    //
    // =================================================================
    //
    // Audio is GLOBAL DEVICE state (one output device, one mixer), so
    // this is a lazy singleton like the ScriptHost: the device starts
    // on first use and stops at process exit. miniaudio runs its own
    // mixer thread; every call here just hands it work, so it's safe
    // to call from gameplay without blocking the frame.
    //
    // Two kinds of playback:
    //   playSound -- fire-and-forget SFX, fully managed by miniaudio
    //                (decoded, mixed, cleaned up when done).
    //   playMusic -- ONE streaming track at a time (decoded on the fly,
    //                not loaded whole); starting a new one replaces the
    //                old. Looping by default.
    //
    // Paths resolve relative to the working directory, which is the
    // PROJECT root -- so "assets/audio/jump.wav" lands in the project.
    //
    // =================================================================

    export class AudioEngine
    {
    public:
        static AudioEngine& instance();

        // One-shot sound effect (fire and forget). No-op if the device
        // failed to initialise or the file can't be read (logged once).
        void playSound(std::string const& path);

        // Start the music track (replacing the current one, if any).
        void playMusic(std::string const& path, bool loop = true);
        void stopMusic();

        // Volumes are linear 0..1. Master scales EVERYTHING (effects and
        // music); music volume scales only the music track.
        void setMasterVolume(float volume);
        void setMusicVolume(float volume);

        // Playback-rate multiplier.
        void setMusicPitch(float pitch);

        [[nodiscard]] bool ready() const;

        ~AudioEngine();
        AudioEngine(AudioEngine const&)            = delete;
        AudioEngine& operator=(AudioEngine const&) = delete;

    private:
        AudioEngine();

        struct Impl;
        std::unique_ptr<Impl> m_impl;

    }; // class AudioEngine

} // namespace Audio