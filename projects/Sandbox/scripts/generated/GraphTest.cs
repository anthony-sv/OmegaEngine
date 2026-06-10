using System.Numerics;
using OmegaEngine;

namespace Game;

// AUTO-GENERATED from a node graph. Do not edit by hand.
public sealed class GraphTest : Script
{
    public override void OnUpdate(float dt)
    {
        if (Input.IsKeyDown(Key.D))
        {
            Entity.Velocity = new Vector2(3f, 0f);
        }
        else
        {
            Console.WriteLine(Time.Elapsed);
        }
    }

}