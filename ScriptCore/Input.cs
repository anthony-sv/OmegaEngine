using System.Numerics;

namespace OmegaEngine;

// Global input facade -- the project-side mirror of the engine's Core::Input.
public static class Input
{
    public static bool IsKeyDown(Key key)         => Interop.IsKeyDown((int)key);
    public static bool WasKeyPressed(Key key)     => Interop.WasKeyPressed((int)key);   // edge: this frame only
    public static bool IsMouseDown(MouseButton b) => Interop.IsMouseDown((int)b);
    public static Vector2 MousePosition           => Interop.MousePosition();
}

// Key codes mirror the engine's Core::Key (GLFW physical-key values). Stored
// as ushort (the highest code is 346); cast to int at the interop boundary.
public enum Key : ushort
{
    Space = 32,

    D0 = 48, D1, D2, D3, D4, D5, D6, D7, D8, D9,

    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    Escape = 256, Enter = 257, Tab = 258, Backspace = 259, Delete = 261,

    Right = 262, Left = 263, Down = 264, Up = 265,

    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    LeftShift = 340, LeftControl = 341, LeftAlt = 342,
    RightShift = 344, RightControl = 345, RightAlt = 346,
}

public enum MouseButton : byte
{
    Left   = 0,
    Right  = 1,
    Middle = 2,
}