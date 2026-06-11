using System.Numerics;

namespace OmegaEngine;

// One-off particle effects for gameplay: `count` particles flung in all
// directions from `position`, fading and shrinking out over `lifetime`.
// They live in the engine's WORLD pool, so the effect survives the entity
// that fired it (a collected coin's sparkle keeps falling).
//
// Continuous effects (torches, fountains, ambience) are authored instead:
// a Particle Emitter component on an entity, set up in the editor.
public static class Particles
{
    public static void Burst(
        Vector2 position,
        int     count,
        Vector4 color,
        float   speed    = 2.0f,
        float   lifetime = 0.5f,
        float   size     = 0.1f,
        string? texture  = null)
        => Interop.ParticlesBurst(position, count, color, speed, lifetime, size, texture);
}