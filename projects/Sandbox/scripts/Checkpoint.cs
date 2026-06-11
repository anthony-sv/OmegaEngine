using System.Numerics;
using OmegaEngine;

namespace Game;

// A checkpoint anchor. Touching it records WHERE the player is in the save
// data -- not as raw coordinates, but as "this scene, this anchor" (the way
// checkpoint games do it: respawn points are hand-placed anchors, so saves
// stay valid even when the level is edited).
//
//   save:    OnTriggerEnter -> checkpoint.scene + checkpoint.tag (+ count)
//   restore: OnCreate -> if THIS checkpoint is the saved anchor for THIS
//            scene, teleport the player here (the menu's Continue option
//            loads the saved scene, which lands in this hook).
//
// Multiple checkpoints in one scene each get a unique Tag (set it in the
// inspector); only the one matching the save claims the player.
public sealed class Checkpoint : Script
{
    public string Player = "Mario";
    public string Tag    = "checkpoint_1";

    public override void OnCreate()
    {
        if (Save.GetString("checkpoint.scene") != Scene.Current || Save.GetString("checkpoint.tag") != Tag)
            return;

        var player = Entity.Find(Player);
        if (!player.IsValid)
            return;

        player.Teleport(Entity.Position);
        player.Velocity = Vector2.Zero;   // clean respawn -- no carried momentum
        Console.WriteLine($"[C#] checkpoint '{Tag}': respawned {Player} at {Entity.Position}");
    }

    public override void OnTriggerEnter(Entity other)
    {
        if (other.Id != Entity.Find(Player).Id)
            return;

        // Re-saving the same anchor repeatedly is fine but noisy -- skip it.
        if (Save.GetString("checkpoint.scene") == Scene.Current && Save.GetString("checkpoint.tag") == Tag)
            return;

        Save.SetString("checkpoint.scene", Scene.Current);
        Save.SetString("checkpoint.tag", Tag);
        Save.SetInt("checkpoint.count", Save.GetInt("checkpoint.count") + 1);
        Audio.Play("assets/audio/checkpoint.wav");
        Particles.Burst(Entity.Position, 16, new Vector4(0.3f, 1.0f, 0.4f, 1.0f),
                        speed: 2.8f, lifetime: 0.7f, size: 0.1f,
                        texture: "assets/textures/particle.png");
        Console.WriteLine($"[C#] checkpoint '{Tag}' saved in scene '{Scene.Current}'");
    }
}