module;

#include "glm/glm.hpp"

export module Engine.ECS:ParticleComponents;

import Engine.Renderer;   // Texture2D (resolved from texturePath, like SpriteRenderer)
import std;

namespace Engine::ECS
{

    // -----------------------------------------------------------------
    // Particle -- one live particle. Pure simulation state: spawned by
    // an emitter (or a script burst), integrated by the ParticleSystem,
    // drawn by interpolating start -> end visuals over its lifetime.
    // -----------------------------------------------------------------

    export struct Particle
    {
        glm::vec2 position      { 0.0f, 0.0f };
        glm::vec2 velocity      { 0.0f, 0.0f };
        float     rotation      { 0.0f };          // degrees
        float     rotationSpeed { 0.0f };          // degrees / second
        float     life          { 0.0f };          // seconds REMAINING
        float     lifetime      { 1.0f };          // seconds total
        glm::vec4 startColor    { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 endColor      { 1.0f, 1.0f, 1.0f, 0.0f };
        float     startSize     { 0.1f };
        float     endSize       { 0.0f };
	}; // struct Particle

    // -----------------------------------------------------------------
    // ParticleEmitterComponent -- a CONTINUOUS particle source attached
    // to an entity (authored in the inspector, serialized in the scene).
    // Particles spawn at the entity's Transform and live in WORLD space
    // (they don't follow the entity afterwards). One-off effects from
    // gameplay (dust, sparkles) use the script `Particles.Burst` API
    // instead, whose particles live in the system's world pool.
    //
    //   rate              : particles spawned per second.
    //   lifetime/speed    : per-particle random in [min, max].
    //   direction, spread : launch angle (degrees, CCW from +X; up = 90)
    //                       +/- a random spread either side.
    //   gravity           : constant acceleration on live particles.
    //   start/end *       : visuals lerped over each particle's life.
    //   texturePath       : sprite for each particle; empty = soft quad.
    //
    // The pool and accumulator are RUNTIME state (not serialized).
    // -----------------------------------------------------------------

    export struct ParticleEmitterComponent
    {
        bool        emitting     { true };
        float       rate         { 12.0f };
        float       lifetimeMin  { 0.6f };
        float       lifetimeMax  { 1.2f };
        float       speedMin     { 0.5f };
        float       speedMax     { 1.5f };
        float       direction    { 90.0f };
        float       spread       { 25.0f };
        glm::vec2   gravity      { 0.0f, 0.0f };
        glm::vec4   startColor   { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4   endColor     { 1.0f, 1.0f, 1.0f, 0.0f };
        float       startSize    { 0.12f };
        float       endSize      { 0.0f };
        std::string texturePath  {};
        int         maxParticles { 256 };

        // -- runtime --
        std::vector<Particle>      pool        {};
        float                      accumulator { 0.0f };
        Renderer::Texture2D const* texture     { nullptr };
	}; // struct ParticleEmitterComponent

} // namespace ECS