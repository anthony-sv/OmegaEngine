module;

#include "glm/glm.hpp"

export module Engine.Systems:ParticleSystem;

import Engine.Core;       // ISystem, Application (assets)
import Engine.ECS;
import Engine.Renderer;
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: ParticleSystem
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Systems
{

    // =================================================================
    //
    //  ParticleSystem -- spawns, integrates and draws particles.
    //
    // =================================================================
    //
    // Two particle homes:
    //
    //   EMITTER pools -- each ParticleEmitterComponent owns its pool;
    //   spawning follows the entity, the particles themselves live in
    //   world space. Authored, continuous effects.
    //
    //   WORLD pool -- a system-wide STATIC pool fed by `burst()` (the
    //   script `Particles.Burst` API lands here). World-owned so a
    //   burst OUTLIVES the entity that fired it -- the coin can vanish
    //   while its sparkle is still falling. Static is safe for the
    //   same reason the script context is: only the active world's
    //   systems tick. onInit clears it so a scene starts clean.
    //
    // Update runs on the fixed step (an ISystem); `render` is a static
    // pass the layers call between sprites and text.
    //
    // =================================================================

    export class ParticleSystem final : public Core::ISystem
    {
        // A world-pool particle carries its own texture (bursts may differ).
        struct WorldParticle
        {
            ECS::Particle              p;
            Renderer::Texture2D const* texture { nullptr };
		}; // struct WorldParticle

    public:

        // A script-requested one-off burst: `count` particles flung in all
        // directions from `position`, fading/shrinking out over `lifetime`.
        struct Burst
        {
            glm::vec2   position    { 0.0f, 0.0f };
            int         count       { 8 };
            glm::vec4   color       { 1.0f, 1.0f, 1.0f, 1.0f };
            float       speed       { 2.0f };
            float       lifetime    { 0.5f };
            float       size        { 0.1f };
            std::string texturePath {};
		}; // struct Burst

        static void burst(Burst const& b)
        {
            Renderer::Texture2D const* texture = b.texturePath.empty()
                ? nullptr
                : Core::Application::get().assets().load<Renderer::Texture2D>(b.texturePath);

            for (int i = 0; i < b.count; ++i)
            {
                float const angle = uniform(0.0f, 360.0f) * std::numbers::pi_v<float> / 180.0f;
                float const speed = uniform(0.5f, 1.0f) * b.speed;

                ECS::Particle p;
                p.position      = b.position;
                p.velocity      = { std::cos(angle) * speed, std::sin(angle) * speed };
                p.rotationSpeed = uniform(-180.0f, 180.0f);
                p.lifetime      = uniform(0.7f, 1.3f) * b.lifetime;
                p.life          = p.lifetime;
                p.startColor    = b.color;
                p.endColor      = { b.color.r, b.color.g, b.color.b, 0.0f };
                p.startSize     = b.size;
                p.endSize       = 0.0f;
                s_worldPool.push_back({ p, texture });
            }
        }

        // Drop every world-pool particle (scene switches; the editor also
        // calls this on Stop so effects don't linger into edit mode).
        static void clearWorld()
        {
            s_worldPool.clear();
        }

        // Fresh scene -> no particles carried over from the last one.
        void onInit() override
        {
            clearWorld();
        }

        void onUpdate(float dt) override
        {
            auto& reg = m_registry;

            // -- emitters: spawn by rate, then integrate their pools --
            for (auto&& [e, tf, em] : reg.view<ECS::Transform, ECS::ParticleEmitterComponent>().each())
            {
                // Resolve the sprite once (same lazy pattern as SpriteRenderer).
                if (em.texture == nullptr && !em.texturePath.empty())
                    em.texture = Core::Application::get().assets().load<Renderer::Texture2D>(em.texturePath);

                if (em.emitting && !reg.hasComponent<ECS::Disabled>(e))
                {
                    em.accumulator += em.rate * dt;
                    while (em.accumulator >= 1.0f
                           && static_cast<int>(em.pool.size()) < em.maxParticles)
                    {
                        em.accumulator -= 1.0f;
                        em.pool.push_back(spawnFrom(em, tf.position));
                    }
                    // Saturated pool: drop the surplus instead of stockpiling it.
                    em.accumulator = std::min(em.accumulator, 1.0f);
                }

                integrate(em.pool, em.gravity, dt);
            }

            // -- world pool (script bursts): mild gravity reads best --
            constexpr glm::vec2 burstGravity { 0.0f, -3.0f };
            for (auto& wp : s_worldPool)
            {
                wp.p.velocity += burstGravity * dt;
                wp.p.position += wp.p.velocity * dt;
                wp.p.rotation += wp.p.rotationSpeed * dt;
                wp.p.life     -= dt;
            }
            std::erase_if(s_worldPool, [](WorldParticle const& wp) { return wp.p.life <= 0.0f; });
        }

        // Draw every live particle (emitter pools + world pool). Called by
        // the layers between the sprite pass and the text pass.
        static void render(ECS::Registry& registry, Renderer::Camera2D const& camera)
        {
            Renderer::Renderer2D::beginBatch(camera);

            for (auto&& [e, em] : registry.view<ECS::ParticleEmitterComponent>().each())
                for (auto const& p : em.pool)
                    drawParticle(p, em.texture);

            for (auto const& wp : s_worldPool)
                drawParticle(wp.p, wp.texture);

            Renderer::Renderer2D::endBatch();
        }

        // The Scene injects its Registry& here (addSystem passes it as
        // the first constructor argument automatically).
        explicit ParticleSystem(ECS::Registry& registry)
            : m_registry { registry }
        {}

    private:

        static float uniform(float lo, float hi)
        {
            static std::mt19937 s_rng { std::random_device {}() };
            return std::uniform_real_distribution<float> { lo, hi }(s_rng);
        }

        static ECS::Particle spawnFrom(ECS::ParticleEmitterComponent const& em, glm::vec2 origin)
        {
            float const angle = (em.direction + uniform(-em.spread, em.spread))
                              * std::numbers::pi_v<float> / 180.0f;
            float const speed = uniform(em.speedMin, em.speedMax);

            ECS::Particle p;
            p.position      = origin;
            p.velocity      = { std::cos(angle) * speed, std::sin(angle) * speed };
            p.rotationSpeed = uniform(-90.0f, 90.0f);
            p.lifetime      = uniform(em.lifetimeMin, em.lifetimeMax);
            p.life          = p.lifetime;
            p.startColor    = em.startColor;
            p.endColor      = em.endColor;
            p.startSize     = em.startSize;
            p.endSize       = em.endSize;
            return p;
        }

        static void integrate(std::vector<ECS::Particle>& pool, glm::vec2 gravity, float dt)
        {
            for (auto& p : pool)
            {
                p.velocity += gravity * dt;
                p.position += p.velocity * dt;
                p.rotation += p.rotationSpeed * dt;
                p.life     -= dt;
            }
            std::erase_if(pool, [](ECS::Particle const& p) { return p.life <= 0.0f; });
        }

        static void drawParticle(ECS::Particle const& p, Renderer::Texture2D const* texture)
        {
            // 0 at birth -> 1 at death; visuals lerp along it.
            float const     t     = 1.0f - p.life / p.lifetime;
            glm::vec4 const color = glm::mix(p.startColor, p.endColor, t);
            float const     size  = std::lerp(p.startSize, p.endSize, t);
            if (size <= 0.0f || color.a <= 0.0f)
                return;

            // drawQuad positions are the BOTTOM-LEFT corner; centre on p.
            glm::vec2 const corner { p.position.x - size * 0.5f, p.position.y - size * 0.5f };
            if (texture != nullptr)
                Renderer::Renderer2D::drawRotatedQuad(corner, { size, size }, p.rotation, *texture, color);
            else
                Renderer::Renderer2D::drawRotatedQuad(corner, { size, size }, p.rotation, color);
        }

        // The world pool: see the class comment for why it's static.
        static inline std::vector<WorldParticle> s_worldPool {};

        ECS::Registry& m_registry;

    }; // class ParticleSystem

} // namespace Systems