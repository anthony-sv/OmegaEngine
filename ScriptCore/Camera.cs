using System.Numerics;

namespace OmegaEngine;

// The view camera, for scripts like a follow-cam. Maps to the engine's
// Camera2D; no-ops if the host has no camera bound.
public static class Camera
{
    public static Vector2 Position { get => Interop.GetCameraPosition(); set => Interop.SetCameraPosition(value); }
    public static float   Zoom     { get => Interop.GetCameraZoom();     set => Interop.SetCameraZoom(value); }
}