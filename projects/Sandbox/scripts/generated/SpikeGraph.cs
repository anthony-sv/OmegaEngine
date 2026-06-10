using System.Numerics;
using OmegaEngine;

namespace Game;

// AUTO-GENERATED from a node graph (NodeGen spike). Do not edit by hand.
public sealed class SpikeGraph : Script
{
    public float Speed = 3f;

    public override void OnCreate()
    {
        Console.WriteLine("[C#] SpikeGraph: generated from a node graph, alive!");
    }

    public override void OnUpdate(float dt)
    {
        Entity.Position += new Vector2((((Input.IsKeyDown(Key.D) ? 1f : 0f) - (Input.IsKeyDown(Key.A) ? 1f : 0f)) * (Speed * dt)), 0f);
    }
}
