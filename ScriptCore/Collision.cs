using System.Numerics;

namespace OmegaEngine;

// Passed to OnCollisionEnter/Exit. `Normal` is the contact normal oriented to
// point AWAY from `Other` (so it points up when you're standing on something --
// `Normal.Y > 0.5` is a reliable "grounded" test).
public readonly struct Collision(Entity other, Vector2 normal)
{
    public Entity  Other  { get; } = other;
    public Vector2 Normal { get; } = normal;
}