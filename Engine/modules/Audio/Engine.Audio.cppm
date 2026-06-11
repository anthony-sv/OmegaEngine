export module Engine.Audio;

// Audio layer -- a thin, swappable wrapper over miniaudio.
//
//   Engine.Audio = playback (AudioEngine wraps ma_engine: one-shot
//                  sound effects, one streaming music track, volumes)
//
// miniaudio appears ONLY inside Engine.Audio-AudioEngine.cpp -- never in
// an interface -- so consumers see clean std types and the backend can
// be replaced without touching them. Scripts reach it through the
// ScriptHost's Audio_* bindings (the `Audio` class in C#).

export import :AudioEngine;     // the ma_engine wrapper (PIMPL singleton)