using System.Numerics;
using OmegaEngine;

namespace Game;

// A trigger pickup: when the collector (the player, by name) overlaps this
// entity's sensor, it's collected and removed.
public sealed class Pickup : Script
{
    public string Collector = "Mario";

    public override void OnTriggerEnter(Entity other)
    {
        if (other.Id == Entity.Find(Collector).Id)
        {
            Console.WriteLine($"[C#] pickup collected by {Collector}");
            Audio.Play("assets/audio/coin.wav");

            // World-pool burst: the sparkle keeps falling after this entity
            // is destroyed on the next line.
            Particles.Burst(Entity.Position, 14, new Vector4(1.0f, 0.85f, 0.2f, 1.0f),
                            speed: 2.5f, lifetime: 0.6f, size: 0.09f,
                            texture: "assets/textures/particle.png");
            Entity.Destroy();
        }
    }
}