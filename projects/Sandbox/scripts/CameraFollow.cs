using System.Numerics;
using OmegaEngine;

namespace Game;

// Smoothly centres the view camera on a target entity (found by name). Attach
// to any entity in the scene; it just drives the global Camera.
public sealed class CameraFollow : Script
{
    public string  Target    = "Mario";
    public float   Smoothing = 6.0f;
    public Vector2 Offset    = new(0.0f, 0.5f);

    private Entity _target;

    public override void OnCreate() => _target = Entity.Find(Target);

    public override void OnUpdate(float dt)
    {
        if (!_target.IsValid)
        {
            _target = Entity.Find(Target);
            if (!_target.IsValid)
                return;
        }

        var desired = _target.Position + Offset;
        Camera.Position = Vector2.Lerp(Camera.Position, desired, MathF.Min(1.0f, Smoothing * dt));
    }
}