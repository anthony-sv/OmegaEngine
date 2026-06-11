namespace OmegaEngine;

// Sound playback. Paths resolve relative to the PROJECT root (the engine's
// working directory), e.g. "assets/audio/jump.wav".
//
//   Play      -- fire-and-forget sound effect (overlapping plays are fine).
//   PlayMusic -- ONE streaming music track at a time; starting a new one
//                replaces the old. Loops by default.
//
// Volumes are linear 0..1: master scales everything, music only the track.
public static class Audio
{
    public static void Play(string path)                  => Interop.AudioPlay(path);
    public static void PlayMusic(string path, bool loop = true) => Interop.AudioPlayMusic(path, loop);
    public static void StopMusic()                        => Interop.AudioStopMusic();
    public static void SetMasterVolume(float volume)      => Interop.AudioSetMasterVolume(volume);
    public static void SetMusicVolume(float volume)       => Interop.AudioSetMusicVolume(volume);
}