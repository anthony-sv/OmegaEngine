export module Engine.Systems:RenderSystem;

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
*  half                      short      ΩMEGAENGINE :: RenderSystem
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
    //  RenderSystem -- draws every drawable entity in one batch.
    //
    // =================================================================
    //
    // WHAT IS A "SYSTEM"?
    //
    //   In ECS, a system is the LOGIC half. Components are data; a
    //   system reads/writes that data for every entity that matches
    //   a query. RenderSystem's query is "all entities that have BOTH
    //   a Transform (where/how big) and a SpriteRenderer (what it
    //   looks like)". For each match, it submits one quad to the
    //   batch renderer.
    //
    // WHY A STANDALONE CLASS (and not an ISystem)?
    //
    //   Engine.Core's ISystem is an UPDATE-phase abstraction:
    //   onUpdate(float dt), no camera, no registry handle. Rendering
    //   is different on all three counts -- it runs in the RENDER
    //   phase, needs a Camera2D, and needs the Registry to query.
    //   So RenderSystem is a focused, stateless class (like Renderer2D)
    //   that the layer calls during its render pass. ISystem stays for
    //   the update-phase logic systems (movement, lifetimes, etc.).
    //
    // OWNERSHIP OF THE BATCH:
    //
    //   render() owns a COMPLETE batch pass: beginBatch -> draw all
    //   matching entities -> endBatch. Call it once per frame, inside
    //   an active framebuffer/viewport, with the camera you want to
    //   view the scene through. It does not clear the screen or bind
    //   render targets -- that stays the caller's responsibility.
    //
    // =================================================================

    export class RenderSystem
    {
    public:

        // Draw every entity with Transform + SpriteRenderer, as seen
        // through `camera`. Wraps a full beginBatch/endBatch pass.
        //
        //   RenderSystem::render(scene.registry(), editorCamera);
        //
        // Entities carrying the Disabled tag are skipped.
        static void render(ECS::Registry& registry, Renderer::Camera2D const& camera)
        {
            Renderer::Renderer2D::beginBatch(camera);

            // view<...>().each() yields [entity, components...] for every
            // entity that has ALL listed components. The references point
            // straight into EnTT's packed component arrays -- no copies.
            auto view = registry.view<ECS::Transform, ECS::SpriteRenderer>();

            for (auto&& [entity, transform, sprite] : view.each())
            {
                // Skip temporarily-disabled entities. (A future
                // optimization is to bake this into the query with
                // entt::exclude<Disabled> so disabled entities are
                // never visited at all.)
                if (registry.hasComponent<ECS::Disabled>(entity))
                    continue;

                drawEntity(transform, sprite);
            }

            Renderer::Renderer2D::endBatch();
        }

        // Static-only class -- no instances.
        RenderSystem() = delete;


    private:

        // Submit a single entity's quad to the open batch. Picks the
        // right Renderer2D overload based on whether the sprite is
        // textured and whether the transform is rotated.
        static void drawEntity(
            ECS::Transform      const&   transform,
            ECS::SpriteRenderer const&   sprite
        )
        {
            bool const rotated = (transform.rotation != 0.0f);

            if (sprite.texture != nullptr)
            {
                // Textured sprite. SpriteRenderer stores a raw texture
                // pointer plus a UV sub-region; wrap them in a transient
                // SubTexture2D so we can reuse the atlas-aware draw path.
                // SubTexture2D is a lightweight value (pointer + 2 vec2s),
                // so constructing one per draw is effectively free.
                Renderer::SubTexture2D const sub {
                    *sprite.texture, sprite.uvMin, sprite.uvMax
                };

                if (rotated)
                    Renderer::Renderer2D::drawRotatedQuad(
                        transform.position, transform.scale, transform.rotation,
                        sub, sprite.color, sprite.tilingFactor);
                else
                    Renderer::Renderer2D::drawQuad(
                        transform.position, transform.scale,
                        sub, sprite.color, sprite.tilingFactor);
            }
            else
            {
                // Color-only quad (the batch renderer's internal 1x1
                // white texture turns white * color into a flat color).
                if (rotated)
                    Renderer::Renderer2D::drawRotatedQuad(
                        transform.position, transform.scale, transform.rotation,
                        sprite.color);
                else
                    Renderer::Renderer2D::drawQuad(
                        transform.position, transform.scale, sprite.color);
            }
        }

    }; // class RenderSystem

} // namespace Systems