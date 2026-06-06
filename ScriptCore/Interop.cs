using System.Numerics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace OmegaEngine;

// The engine's function table, handed to managed code at Init. Each field is
// a pointer to a native engine function the scripting API calls back into.
// cdecl is explicit (and positions cross by pointer) to keep the ABI exact.
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeApi
{
    public delegate* unmanaged[Cdecl]<uint, Vector2*, void> GetPosition;
    public delegate* unmanaged[Cdecl]<uint, Vector2*, void> SetPosition;
}

// Thin marshalling layer between the C# component API and the native table.
internal static unsafe class Interop
{
    public static NativeApi Api;

    public static Vector2 GetPosition(uint entity)
    {
        Vector2 value;
        Api.GetPosition(entity, &value);
        return value;
    }

    public static void SetPosition(uint entity, Vector2 position)
        => Api.SetPosition(entity, &position);
}