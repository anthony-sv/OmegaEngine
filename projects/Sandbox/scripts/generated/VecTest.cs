using System.Numerics;
using OmegaEngine;

namespace Game;

// AUTO-GENERATED from a node graph. Do not edit by hand.
public sealed class VecTest : Script
{
    public float speed = 2.5f;

    public override void OnUpdate(float dt)
    {
        Entity.Position += (new Vector2(speed, 0f) * dt);
    }

    public override void OnCollisionEnter(Collision collision)
    {
        Console.WriteLine(collision.Normal.Y);
    }

}