module;

#include "glm/glm.hpp"

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
        //   RenderSystem::render(scene.registry(), editorCamera, alpha);
        //
        // `alpha` is the fixed-timestep sub-step fraction (GameLoop::
        // alpha(), [0,1)). When an entity also has a PreviousTransform
        // (maintained by InterpolationSystem), it is drawn at
        // lerp(previous, current, alpha) for smooth motion above the sim
        // rate. alpha defaults to 1.0 (= draw the current Transform), so
        // callers that don't interpolate keep working unchanged.
        // Entities carrying the Disabled tag are skipped.
        static void render(ECS::Registry& registry, Renderer::Camera2D const& camera, float alpha = 1.0f)
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

                drawEntity(interpolated(registry, entity, transform, alpha), sprite);
            }

            Renderer::Renderer2D::endBatch();
        }

        // Draw every TilemapComponent's non-empty cells, as seen through
        // `camera`. Tilemaps are the BACKGROUND layer, so call this BEFORE
        // render() -- sprites then draw on top. Wraps its own batch pass.
        // (No interpolation: tilemaps don't move per-frame.)
        static void renderTilemaps(ECS::Registry& registry, Renderer::Camera2D const& camera)
        {
            Renderer::Renderer2D::beginBatch(camera);

            for (auto&& [entity, transform, map] :
                 registry.view<ECS::Transform, ECS::TilemapComponent>().each())
            {
                if (registry.hasComponent<ECS::Disabled>(entity))
                    continue;
                drawTilemap(transform, map);
            }

            Renderer::Renderer2D::endBatch();
        }

        // Draw the CELL GRID of every tilemap as faint thin lines -- an
        // editor authoring aid so you can see where cells fall. Call after
        // renderTilemaps, into the same target. (Editor overlay; the runtime
        // doesn't draw it.)
        static void renderTilemapGrid(ECS::Registry& registry, Renderer::Camera2D const& camera)
        {
            constexpr glm::vec4 line  { 1.0f, 1.0f, 1.0f, 0.22f };
            constexpr float     thick = 0.012f;

            Renderer::Renderer2D::beginBatch(camera);

            for (auto&& [entity, transform, map] :
                 registry.view<ECS::Transform, ECS::TilemapComponent>().each())
            {
                float     const ts   = map.tileWorldSize;
                glm::vec2 const o    = transform.position;        // grid origin (bottom-left)
                glm::vec2 const span { map.dimensions.x * ts, map.dimensions.y * ts };

                for (int x = 0; x <= map.dimensions.x; ++x)       // vertical lines
                {
                    float const px = o.x + static_cast<float>(x) * ts;
                    drawLine({ px, o.y }, { px, o.y + span.y }, thick, line);
                }
                for (int y = 0; y <= map.dimensions.y; ++y)       // horizontal lines
                {
                    float const py = o.y + static_cast<float>(y) * ts;
                    drawLine({ o.x, py }, { o.x + span.x, py }, thick, line);
                }
            }

            Renderer::Renderer2D::endBatch();
        }

        // Draw a WIREFRAME overlay of every collider (box/circle/polygon)
        // as seen through `camera`. Reads the ECS collider COMPONENTS, not
        // Box2D bodies, so it works in the editor's Edit mode too (where
        // no simulation is running) -- ideal for checking that a collider
        // matches its sprite. Triggers (sensors) draw yellow, solids green.
        // Call after render(), into the same target. `alpha` interpolates
        // the same way as render() so the wireframe tracks the sprite.
        static void renderColliders(ECS::Registry& registry, Renderer::Camera2D const& camera, float alpha = 1.0f)
        {
            constexpr glm::vec4 solid  { 0.25f, 0.90f, 0.35f, 1.0f };
            constexpr glm::vec4 sensor { 0.95f, 0.85f, 0.20f, 1.0f };
            constexpr float     thick = 0.025f;

            Renderer::Renderer2D::beginBatch(camera);

            for (auto&& [e, tf, c] : registry.view<ECS::Transform, ECS::BoxCollider2D>().each())
            {
                auto const t = interpolated(registry, e, tf, alpha);
                drawBoxOutline(
                    t.position + rotateVec(c.offset, t.rotation),
                    c.size, t.rotation,
                    c.isTrigger ? sensor : solid,
                    thick
                );
            }

            for (auto&& [e, tf, c] : registry.view<ECS::Transform, ECS::CircleCollider2D>().each())
            {
                auto const t = interpolated(registry, e, tf, alpha);
                drawCircleOutline(
                    t.position + rotateVec(c.offset, t.rotation),
                    c.radius,
                    c.isTrigger ? sensor : solid,
                    thick
                );
            }

            for (auto&& [e, tf, c] : registry.view<ECS::Transform, ECS::PolygonCollider2D>().each())
            {
                auto const t = interpolated(registry, e, tf, alpha);
                auto const n = c.points.size();
                for (std::size_t i = 0; i < n; ++i)
                    drawLine(
                        t.position + rotateVec(c.points[i], t.rotation),
                        t.position + rotateVec(c.points[(i + 1) % n], t.rotation),
                        thick,
                        c.isTrigger ? sensor : solid
                    );
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

            // Transform.position is the entity CENTER (consistent with the
            // physics body and the collider). The batch renderer's draw
            // overloads take the bottom-left corner and rotate about the
            // quad centre, so shift by -scale/2 to centre the sprite on
            // position. (Rotation then pivots about position too.)
            glm::vec2 const drawPos = transform.position - transform.scale * 0.5f;

            if (sprite.texture != nullptr)
            {
                glm::vec2 uvMin = sprite.uvMin;
                glm::vec2 uvMax = sprite.uvMax;

                // HALF-TEXEL INSET (atlas anti-bleed). When a sprite uses
                // a SUB-REGION of an atlas (uv != the full [0,1] rect), a
                // texel sampled exactly at the cell boundary can land in
                // the NEIGHBOURING cell -- showing up as a dark/garbage
                // edge, most visible under minification (small windows /
                // low-DPI displays). Pulling the UV rect in by half a
                // texel keeps sampling strictly inside the cell. We skip
                // full-texture sprites (no neighbour) and tiling sprites
                // (uv stays [0,1]; tilingFactor repeats in the shader), so
                // seamless tiling is unaffected.
                bool const atlas = (
                    uvMin != glm::vec2 { 0.0f, 0.0f } 
                    || uvMax != glm::vec2 { 1.0f, 1.0f }
                );
                if (atlas)
                {
                    glm::vec2 const halfTexel = 0.5f / glm::vec2 {
                        static_cast<float>(sprite.texture->width()),
                        static_cast<float>(sprite.texture->height())
                    };
                    uvMin += halfTexel;
                    uvMax -= halfTexel;
                }

                // Textured sprite. SpriteRenderer stores a raw texture
                // pointer plus a UV sub-region; wrap them in a transient
                // SubTexture2D so we can reuse the atlas-aware draw path.
                // SubTexture2D is a lightweight value (pointer + 2 vec2s),
                // so constructing one per draw is effectively free.
                Renderer::SubTexture2D const sub {
                    *sprite.texture, 
                    uvMin, 
                    uvMax
                };

                if (rotated)
                    Renderer::Renderer2D::drawRotatedQuad(
                        drawPos, 
                        transform.scale, 
                        transform.rotation,
                        sub, 
                        sprite.color, 
                        sprite.tilingFactor
                    );
                else
                    Renderer::Renderer2D::drawQuad(
                        drawPos, 
                        transform.scale,
                        sub, 
                        sprite.color, 
                        sprite.tilingFactor
                    );
            }
            else
            {
                // Color-only quad (the batch renderer's internal 1x1
                // white texture turns white * color into a flat color).
                if (rotated)
                    Renderer::Renderer2D::drawRotatedQuad(
                        drawPos, 
                        transform.scale, 
                        transform.rotation,
                        sprite.color
                    );
                else
                    Renderer::Renderer2D::drawQuad(
                        drawPos, 
                        transform.scale, 
                        sprite.color
                    );
            }
        }

        // Draw one tilemap: one quad per non-empty cell. Branches on the
        // map's source mode (Atlas = slice one texture; Collection = one
        // whole texture per tile id). `position` is the grid ORIGIN and
        // drawQuad takes the bottom-left, so cell (x,y) sits at
        // position + (x,y)*tileWorldSize.
        static void drawTilemap(ECS::Transform const& transform, ECS::TilemapComponent const& map)
        {
            using Source = ECS::TilemapComponent::Source;

            if (map.tiles.size() != map.cellCount())
                return;     // grid not sized yet -- nothing safe to draw

            float const ts = map.tileWorldSize;
            auto const  g  = map.tilesView();   // 2D [y, x] view over the flat grid

            // -- Atlas: precompute the slicing constants once --------------
            int       columns   = 1;
            glm::vec2 halfTexel  { 0.0f, 0.0f };
            if (map.source == Source::Atlas)
            {
                if (map.atlas == nullptr || map.tilePixelSize.x <= 0 || map.tilePixelSize.y <= 0)
                    return;
                columns   = std::max(1, static_cast<int>(map.atlas->width()) / map.tilePixelSize.x);
                halfTexel = 0.5f / glm::vec2 {
                    static_cast<float>(map.atlas->width()),
                    static_cast<float>(map.atlas->height())
                };
            }

            for (int y = 0; y < map.dimensions.y; ++y)
            {
                for (int x = 0; x < map.dimensions.x; ++x)
                {
                    int const id = g[static_cast<std::size_t>(y), static_cast<std::size_t>(x)];
                    if (id < 0)
                        continue;

                    glm::vec2 const cellBL = transform.position
                        + glm::vec2 { static_cast<float>(x), static_cast<float>(y) } * ts;

                    if (map.source == Source::Atlas)
                    {
                        // Slice the atlas cell; half-texel inset avoids bleed.
                        auto const sub = Renderer::SubTexture2D::createFromGrid(
                            *map.atlas,
                            glm::vec2 { static_cast<float>(id % columns), static_cast<float>(id / columns) },
                            glm::vec2 {
                                static_cast<float>(map.tilePixelSize.x),
                                static_cast<float>(map.tilePixelSize.y)
                            });
                        Renderer::SubTexture2D const inset {
                            *map.atlas, sub.uvMin() + halfTexel, sub.uvMax() - halfTexel
                        };
                        Renderer::Renderer2D::drawQuad(cellBL, { ts, ts }, inset);
                    }
                    else // Source::Collection -- a tile DEF (texture + uv + footprint)
                    {
                        if (static_cast<std::size_t>(id) >= map.tileDefs.size())
                            continue;
                        auto const& def = map.tileDefs[id];
                        if (def.texture == nullptr)
                            continue;

                        // Span the footprint in cells; anchored bottom-left.
                        glm::vec2 const size {
                            static_cast<float>(std::max(1, def.footprint.x)) * ts,
                            static_cast<float>(std::max(1, def.footprint.y)) * ts
                        };
                        Renderer::SubTexture2D const sub { *def.texture, def.uvMin, def.uvMax };
                        Renderer::Renderer2D::drawQuad(cellBL, size, sub);
                    }
                }
            }
        }

        // -- Render interpolation -------------------------------------

        // Shortest-arc angle lerp (degrees). Naive lerp across the
        // 360->0 wrap would spin the long way round (e.g. 350->10 would
        // sweep through 180); this picks the <=180 path instead.
        static float lerpAngle(float a, float b, float t)
        {
            float const delta = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;   // -> [-180, 180]
            return a + delta * t;
        }

        // Return `current` blended toward the entity's PreviousTransform
        // by alpha (position linearly, rotation shortest-arc). If the
        // entity has no PreviousTransform (e.g. edit mode, or freshly
        // created), returns `current` unchanged. `entity` is templated to
        // avoid naming entt at this layer.
        static ECS::Transform interpolated(
            ECS::Registry& registry, auto entity, ECS::Transform const& current, float alpha)
        {
            ECS::Transform out = current;
            if (registry.hasComponent<ECS::PreviousTransform>(entity))
            {
                auto const& prev = registry.getComponent<ECS::PreviousTransform>(entity);
                out.position = glm::mix(prev.position, current.position, alpha);
                out.rotation = lerpAngle(prev.rotation, current.rotation, alpha);
            }
            return out;
        }


        // -- Debug-draw primitives (lines built from thin rotated quads,
        //    so they reuse the existing quad batch -- no GL line pipeline) --

        // Rotate `v` by `degrees` (CCW), matching Transform's convention.
        static glm::vec2 rotateVec(glm::vec2 v, float degrees)
        {
            float const r = glm::radians(degrees);
            float const c = std::cos(r);
            float const s = std::sin(r);
            return { v.x * c - v.y * s, v.x * s + v.y * c };
        }

        // A line a->b drawn as a thin quad of the given world-space thickness.
        static void drawLine(glm::vec2 a, glm::vec2 b, float thickness, glm::vec4 color)
        {
            glm::vec2 const d   = b - a;
            float     const len = glm::length(d);
            if (len < 1e-6f)
                return;

            float     const angle = glm::degrees(std::atan2(d.y, d.x));
            glm::vec2 const size  { len, thickness };
            glm::vec2 const mid   = (a + b) * 0.5f;

            // drawRotatedQuad takes the bottom-left corner and pivots about
            // the quad centre, so pass (mid - size/2) to centre the thin
            // quad exactly on the segment midpoint.
            Renderer::Renderer2D::drawRotatedQuad(mid - size * 0.5f, size, angle, color);
        }

        static void drawBoxOutline(glm::vec2 center, glm::vec2 size, float rotationDeg, glm::vec4 color, float thick)
        {
            glm::vec2 const h = size * 0.5f;
            glm::vec2 corner[4] = {
                { -h.x, -h.y }, 
                { h.x, -h.y }, 
                { h.x, h.y }, 
                { -h.x, h.y }
            };
            for (auto& p : corner)
                p = center + rotateVec(p, rotationDeg);

            for (int i = 0; i < 4; ++i)
                drawLine(corner[i], corner[(i + 1) % 4], thick, color);
        }

        static void drawCircleOutline(glm::vec2 center, float radius, glm::vec4 color, float thick)
        {
            constexpr int          segments = 24;
            constexpr float        tau      = 6.283185307f;
            glm::vec2              prev      = center + glm::vec2 { radius, 0.0f };

            for (int i = 1; i <= segments; ++i)
            {
                float const a = (static_cast<float>(i) / segments) * tau;
                glm::vec2 const cur = center + glm::vec2 { std::cos(a), std::sin(a) } * radius;
                drawLine(prev, cur, thick, color);
                prev = cur;
            }
        }

    }; // class RenderSystem

} // namespace Systems