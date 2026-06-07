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
            Entity.Destroy();
        }
    }
}