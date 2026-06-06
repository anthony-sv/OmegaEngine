using System.Numerics;
using OmegaEngine;

namespace Game;

// Demo gameplay script: drifts its entity to the right, proving the engine
// drives entity state from project-side C# each frame.
public sealed class Mover : Script
{
    private float _elapsed;

    public override void OnCreate()
        => Console.WriteLine($"[C#] Mover spawned at {Entity.Position}");

    public override void OnUpdate(float dt)
    {
        Entity.Position += new Vector2(1.5f * dt, 0.0f);   // 1.5 units/s to the right

        if ((_elapsed += dt) >= 1.0f)
        {
            _elapsed = 0.0f;
            Console.WriteLine($"[C#] Mover at {Entity.Position}");
        }
    }
}