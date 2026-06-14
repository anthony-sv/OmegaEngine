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

    // Teleport: place the entity (and its physics body, if it has one) at a
    // point, bypassing the simulation -- for respawns and checkpoints, not
    // movement. Velocity is preserved; zero it for a clean respawn.
    public void Teleport(Vector2 position) => Interop.Teleport(Id, position);

    // Angular axis (wheelies, leaning, spins). Degrees/second, CCW positive
    // -- matching Rotation. Torque follows the same sign.
    public float AngularVelocity { get => Interop.GetAngularVelocity(Id); set => Interop.SetAngularVelocity(Id, value); }
    public void ApplyTorque(float torque) => Interop.ApplyTorque(Id, torque);

    // -- Joint motor (this entity carries a Revolute/Wheel joint) --
    // The drive of a powered wheel: speed is the TARGET (degrees/second),
    // torque is how hard the motor pushes toward it. Speed 0 + torque on
    // is a brake. No-ops without a joint.
    public void SetMotorSpeed(float degreesPerSecond) => Interop.SetMotorSpeed(Id, degreesPerSecond);
    public void SetMotorTorque(float torque)          => Interop.SetMotorTorque(Id, torque);
    public void SetMotorEnabled(bool enabled)         => Interop.EnableMotor(Id, enabled);

    // -- Text (HUD) --
    // Writes a TextComponent's string / colour (gear indicator, speedo, lap
    // timer). The entity must already carry one, authored in the scene.
    public void SetText(string text)        => Interop.SetText(Id, text);
    public void SetTextColor(Vector4 color) => Interop.SetTextColor(Id, color);

    // -- Sprite --
    public Vector4 Color { get => Interop.GetColor(Id); set => Interop.SetColor(Id, value); }
    public void SetTexture(string path) => Interop.SetTexture(Id, path);   // swaps the sprite image (full UVs)
    public void SetFlipX(bool flip)     => Interop.SetFlipX(Id, flip);     // mirror horizontally

    // -- Lifecycle / lookup --
    public void Destroy() => Interop.Destroy(Id);

    public static Entity Create(string name) => new(Interop.Create(name));   // new entity + Transform
    public static Entity Find(string name)   => new(Interop.Find(name));     // first entity with this name
}