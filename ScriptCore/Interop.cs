using System.Numerics;
using System.Runtime.InteropServices;

namespace OmegaEngine;

// The engine's function table, handed to managed code at Init. Each field is a
// pointer to a native engine function the scripting API calls back into. cdecl
// is explicit and vectors cross by pointer to keep the ABI exact.
//
// THIS LAYOUT IS A HARD CONTRACT: the native side fills an identical struct in
// the SAME field order. Add to both lists together, at the end.
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeApi
{
    // transform
    public delegate* unmanaged[Cdecl]<uint, Vector2*, void> GetPosition;
    public delegate* unmanaged[Cdecl]<uint, Vector2*, void> SetPosition;
    public delegate* unmanaged[Cdecl]<uint, float>          GetRotation;
    public delegate* unmanaged[Cdecl]<uint, float, void>    SetRotation;
    public delegate* unmanaged[Cdecl]<uint, Vector2*, void> GetScale;
    public delegate* unmanaged[Cdecl]<uint, Vector2*, void> SetScale;

    // physics body
    public delegate* unmanaged[Cdecl]<uint, Vector2*, void> GetVelocity;
    public delegate* unmanaged[Cdecl]<uint, Vector2*, void> SetVelocity;
    public delegate* unmanaged[Cdecl]<uint, Vector2*, void> ApplyImpulse;
    public delegate* unmanaged[Cdecl]<uint, Vector2*, void> ApplyForce;

    // sprite
    public delegate* unmanaged[Cdecl]<uint, Vector4*, void> GetColor;
    public delegate* unmanaged[Cdecl]<uint, Vector4*, void> SetColor;
    public delegate* unmanaged[Cdecl]<uint, nint, void>     SetTexture;
    public delegate* unmanaged[Cdecl]<uint, int, void>      SetFlipX;

    // input
    public delegate* unmanaged[Cdecl]<int, int>             IsKeyDown;
    public delegate* unmanaged[Cdecl]<int, int>             WasKeyPressed;
    public delegate* unmanaged[Cdecl]<int, int>             IsMouseDown;
    public delegate* unmanaged[Cdecl]<Vector2*, void>       MousePosition;

    // time
    public delegate* unmanaged[Cdecl]<float>                TimeDelta;
    public delegate* unmanaged[Cdecl]<float>                TimeElapsed;

    // entity lifecycle / lookup
    public delegate* unmanaged[Cdecl]<nint, uint>           Create;
    public delegate* unmanaged[Cdecl]<uint, void>           Destroy;
    public delegate* unmanaged[Cdecl]<nint, uint>           Find;
    public delegate* unmanaged[Cdecl]<uint, int>            IsValid;

    // script fields: authored override for (entity, fieldName) -> value string, or 0
    public delegate* unmanaged[Cdecl]<uint, nint, nint>     GetScriptField;

    // camera (the view)
    public delegate* unmanaged[Cdecl]<Vector2*, void>       GetCameraPosition;
    public delegate* unmanaged[Cdecl]<Vector2*, void>       SetCameraPosition;
    public delegate* unmanaged[Cdecl]<float>                GetCameraZoom;
    public delegate* unmanaged[Cdecl]<float, void>          SetCameraZoom;

    // scene / app control
    public delegate* unmanaged[Cdecl]<nint, void>           SceneLoad;
    public delegate* unmanaged[Cdecl]<void>                 AppQuit;

    // teleport + scene query (appended -- keep the ABI order)
    public delegate* unmanaged[Cdecl]<uint, Vector2*, void> Teleport;
    public delegate* unmanaged[Cdecl]<nint>                 SceneName;

    // audio (appended -- keep the ABI order)
    public delegate* unmanaged[Cdecl]<nint, void>           AudioPlay;
    public delegate* unmanaged[Cdecl]<nint, int, void>      AudioPlayMusic;
    public delegate* unmanaged[Cdecl]<void>                 AudioStopMusic;
    public delegate* unmanaged[Cdecl]<float, void>          AudioSetMasterVolume;
    public delegate* unmanaged[Cdecl]<float, void>          AudioSetMusicVolume;

    // particles (appended -- keep the ABI order)
    public delegate* unmanaged[Cdecl]<Vector2*, int, Vector4*, float, float, float, nint, void> ParticlesBurst;

    // angular body control + HUD text (appended -- keep the ABI order)
    public delegate* unmanaged[Cdecl]<uint, float>          GetAngularVelocity;
    public delegate* unmanaged[Cdecl]<uint, float, void>    SetAngularVelocity;
    public delegate* unmanaged[Cdecl]<uint, float, void>    ApplyTorque;
    public delegate* unmanaged[Cdecl]<uint, nint, void>     SetText;

    // music pitch (appended -- keep the ABI order)
    public delegate* unmanaged[Cdecl]<float, void>          AudioSetMusicPitch;

    // joint motors (appended -- keep the ABI order)
    public delegate* unmanaged[Cdecl]<uint, float, void>    SetMotorSpeed;
    public delegate* unmanaged[Cdecl]<uint, float, void>    SetMotorTorque;
    public delegate* unmanaged[Cdecl]<uint, int, void>      EnableMotor;

    // text colour (appended -- keep the ABI order)
    public delegate* unmanaged[Cdecl]<uint, Vector4*, void> SetTextColor;
}

// Thin marshalling layer between the C# API surface and the native table.
// Vectors cross by pointer; booleans as int (0/1); strings as UTF-8 buffers.
internal static unsafe class Interop
{
    public static NativeApi Api;

    // -- transform --
    public static Vector2 GetPosition(uint e) { Vector2 v; Api.GetPosition(e, &v); return v; }
    public static void    SetPosition(uint e, Vector2 v) => Api.SetPosition(e, &v);
    public static float   GetRotation(uint e) => Api.GetRotation(e);
    public static void    SetRotation(uint e, float deg) => Api.SetRotation(e, deg);
    public static Vector2 GetScale(uint e) { Vector2 v; Api.GetScale(e, &v); return v; }
    public static void    SetScale(uint e, Vector2 v) => Api.SetScale(e, &v);

    // -- physics body --
    public static Vector2 GetVelocity(uint e) { Vector2 v; Api.GetVelocity(e, &v); return v; }
    public static void    SetVelocity(uint e, Vector2 v) => Api.SetVelocity(e, &v);
    public static void    ApplyImpulse(uint e, Vector2 v) => Api.ApplyImpulse(e, &v);
    public static void    ApplyForce(uint e, Vector2 v) => Api.ApplyForce(e, &v);

    // -- sprite --
    public static Vector4 GetColor(uint e) { Vector4 v; Api.GetColor(e, &v); return v; }
    public static void    SetColor(uint e, Vector4 v) => Api.SetColor(e, &v);
    public static void    SetFlipX(uint e, bool flip) => Api.SetFlipX(e, flip ? 1 : 0);

    public static void SetTexture(uint e, string path)
    {
        nint utf8 = Marshal.StringToCoTaskMemUTF8(path);
        try { Api.SetTexture(e, utf8); }
        finally { Marshal.FreeCoTaskMem(utf8); }
    }

    // -- input --
    public static bool    IsKeyDown(int key) => Api.IsKeyDown(key) != 0;
    public static bool    WasKeyPressed(int key) => Api.WasKeyPressed(key) != 0;
    public static bool    IsMouseDown(int button) => Api.IsMouseDown(button) != 0;
    public static Vector2 MousePosition() { Vector2 v; Api.MousePosition(&v); return v; }

    // -- time --
    public static float TimeDelta() => Api.TimeDelta();
    public static float TimeElapsed() => Api.TimeElapsed();

    // -- entity lifecycle / lookup --
    public static void Destroy(uint e) => Api.Destroy(e);
    public static bool IsValid(uint e) => Api.IsValid(e) != 0;

    public static uint Create(string name)
    {
        nint utf8 = Marshal.StringToCoTaskMemUTF8(name);
        try { return Api.Create(utf8); }
        finally { Marshal.FreeCoTaskMem(utf8); }
    }

    public static uint Find(string name)
    {
        nint utf8 = Marshal.StringToCoTaskMemUTF8(name);
        try { return Api.Find(utf8); }
        finally { Marshal.FreeCoTaskMem(utf8); }
    }

    public static string? GetScriptField(uint entity, string name)
    {
        nint utf8 = Marshal.StringToCoTaskMemUTF8(name);
        try
        {
            nint result = Api.GetScriptField(entity, utf8);
            return result == 0 ? null : Marshal.PtrToStringUTF8(result);
        }
        finally { Marshal.FreeCoTaskMem(utf8); }
    }

    // -- camera --
    public static Vector2 GetCameraPosition() { Vector2 v; Api.GetCameraPosition(&v); return v; }
    public static void    SetCameraPosition(Vector2 v) => Api.SetCameraPosition(&v);
    public static float   GetCameraZoom() => Api.GetCameraZoom();
    public static void    SetCameraZoom(float z) => Api.SetCameraZoom(z);

    // -- scene / app --
    public static void Quit() => Api.AppQuit();

    public static void LoadScene(string name)
    {
        nint utf8 = Marshal.StringToCoTaskMemUTF8(name);
        try { Api.SceneLoad(utf8); }
        finally { Marshal.FreeCoTaskMem(utf8); }
    }

    public static void Teleport(uint e, Vector2 v) => Api.Teleport(e, &v);

    public static string SceneName()
    {
        nint result = Api.SceneName();
        return result == 0 ? "" : (Marshal.PtrToStringUTF8(result) ?? "");
    }

    // -- audio --
    public static void AudioPlay(string path)
    {
        nint utf8 = Marshal.StringToCoTaskMemUTF8(path);
        try { Api.AudioPlay(utf8); }
        finally { Marshal.FreeCoTaskMem(utf8); }
    }

    public static void AudioPlayMusic(string path, bool loop)
    {
        nint utf8 = Marshal.StringToCoTaskMemUTF8(path);
        try { Api.AudioPlayMusic(utf8, loop ? 1 : 0); }
        finally { Marshal.FreeCoTaskMem(utf8); }
    }

    public static void AudioStopMusic()           => Api.AudioStopMusic();
    public static void AudioSetMasterVolume(float v) => Api.AudioSetMasterVolume(v);
    public static void AudioSetMusicVolume(float v)  => Api.AudioSetMusicVolume(v);
    public static void AudioSetMusicPitch(float v)   => Api.AudioSetMusicPitch(v);

    // -- joint motors --
    public static void SetMotorSpeed(uint e, float v)  => Api.SetMotorSpeed(e, v);
    public static void SetMotorTorque(uint e, float v) => Api.SetMotorTorque(e, v);
    public static void EnableMotor(uint e, bool on)    => Api.EnableMotor(e, on ? 1 : 0);

    // -- particles --
    public static void ParticlesBurst(Vector2 position, int count, Vector4 color,
                                      float speed, float lifetime, float size, string? texture)
    {
        nint utf8 = texture is null ? 0 : Marshal.StringToCoTaskMemUTF8(texture);
        try { Api.ParticlesBurst(&position, count, &color, speed, lifetime, size, utf8); }
        finally { if (utf8 != 0) Marshal.FreeCoTaskMem(utf8); }
    }

    // -- angular body control + HUD text --
    public static float GetAngularVelocity(uint e)          => Api.GetAngularVelocity(e);
    public static void  SetAngularVelocity(uint e, float v) => Api.SetAngularVelocity(e, v);
    public static void  ApplyTorque(uint e, float torque)   => Api.ApplyTorque(e, torque);

    public static void SetText(uint e, string text)
    {
        nint utf8 = Marshal.StringToCoTaskMemUTF8(text);
        try { Api.SetText(e, utf8); }
        finally { Marshal.FreeCoTaskMem(utf8); }
    }

    public static void SetTextColor(uint e, Vector4 c) => Api.SetTextColor(e, &c);
}