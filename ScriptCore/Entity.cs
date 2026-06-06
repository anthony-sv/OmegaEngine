using System.Numerics;

namespace OmegaEngine;

// A handle to a native ECS entity. Component access routes through the
// interop table back into the engine. Cheap value type (just an id).
public readonly struct Entity(uint id)
{
    public uint Id { get; } = id;

    public Vector2 Position
    {
        get => Interop.GetPosition(Id);
        set => Interop.SetPosition(Id, value);
    }
}