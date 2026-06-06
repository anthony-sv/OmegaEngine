using System.Numerics;

namespace OmegaEngine;

// A handle to a native ECS entity. State lives in the engine; every member
// routes through the interop table. Cheap value type (just an id).
public readonly struct Entity(uint id)
{
    public uint Id { get; } = id;

    // True while the entity is alive in the current world.
    public bool IsValid => Interop.IsValid(Id);

    // -- Transform --
    public Vector2 Position { get => Interop.GetPosition(Id); set => Interop.SetPosition(Id, value); }
    public float   Rotation { get => Interop.GetRotation(Id); set => Interop.SetRotation(Id, value); }   // degrees, CCW
    public Vector2 Scale    { get => Interop.GetScale(Id);    set => Interop.SetScale(Id, value); }

    // -- Physics body --
    // For a Dynamic body the simulation owns the Transform, so steer it through
    // velocity / impulse / force rather than writing Position. No-ops if the
    // entity has no physics body.
    public Vector2 Velocity { get => Interop.GetVelocity(Id); set => Interop.SetVelocity(Id, value); }
    public void ApplyImpulse(Vector2 impulse) => Interop.ApplyImpulse(Id, impulse);   // instant kick (jumps)
    public void ApplyForce(Vector2 force)     => Interop.ApplyForce(Id, force);       // continuous push

    // -- Sprite --
    public Vector4 Color { get => Interop.GetColor(Id); set => Interop.SetColor(Id, value); }
    public void SetTexture(string path) => Interop.SetTexture(Id, path);   // swaps the sprite image (full UVs)
    public void SetFlipX(bool flip)     => Interop.SetFlipX(Id, flip);     // mirror horizontally

    // -- Lifecycle / lookup --
    public void Destroy() => Interop.Destroy(Id);

    public static Entity Create(string name) => new(Interop.Create(name));   // new entity + Transform
    public static Entity Find(string name)   => new(Interop.Find(name));     // first entity with this name
}