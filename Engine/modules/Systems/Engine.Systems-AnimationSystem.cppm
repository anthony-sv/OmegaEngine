module;

#include "glm/glm.hpp"

export module Engine.Systems:AnimationSystem;

import Engine.Core;   // Core::ISystem
import Engine.ECS;    // Registry, SpriteAnimation, SpriteRenderer
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: AnimationSystem
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
    //  AnimationSystem -- advances SpriteAnimation into SpriteRenderer.
    //
    // =================================================================
    //
    // An UPDATE-phase Core::ISystem. For every entity that has BOTH a
    // SpriteAnimation (the frame timeline) and a SpriteRenderer (what's
    // drawn), it:
    //
    //   1. accumulates dt into the animation's elapsed time,
    //   2. advances currentFrame whenever a frame's duration elapses
    //      (wrapping if looping, otherwise stopping on the last frame),
    //   3. writes the current frame's UV rect into the SpriteRenderer.
    //
    // It does NOT touch the SpriteRenderer's texture -- the animation
    // only chooses WHICH PART of the sheet is shown. The RenderSystem
    // (render phase, after update) then draws the sprite with whatever
    // UVs we left on it. Clean hand-off: animation writes data, render
    // reads it.
    //
    // The `while` (not `if`) catches up when a frame ran long enough to
    // skip several animation frames; guarded on frameDuration > 0 so a
    // zero duration can't spin forever.
    //
    // =================================================================

    export class AnimationSystem final : public Core::ISystem
    {
    public:

        explicit AnimationSystem(ECS::Registry& registry)
            : m_registry { registry }
        {}

        void onUpdate(float dt) override
        {
            for (
                auto&& [entity, anim, sprite]: m_registry.view<ECS::SpriteAnimation, ECS::SpriteRenderer>().each()
                )
            {
                if (!anim.playing || anim.frames.empty())
                    continue;

                auto const frameCount = static_cast<std::uint32_t>(anim.frames.size());

                if (anim.frameDuration > 0.0f)
                {
                    anim.elapsed += dt;

                    while (anim.elapsed >= anim.frameDuration)
                    {
                        anim.elapsed -= anim.frameDuration;
                        ++anim.currentFrame;

                        if (anim.currentFrame >= frameCount)
                        {
                            if (anim.looping)
                            {
                                anim.currentFrame = 0;
                            }
                            else
                            {
                                // Hold on the last frame and stop.
                                anim.currentFrame = frameCount - 1;
                                anim.playing      = false;
                                break;
                            }
                        }
                    }
                }

                // Safety clamp (e.g. if frames shrank since last tick).
                if (anim.currentFrame >= frameCount)
                    anim.currentFrame = frameCount - 1;

                // Hand the chosen frame's UV rect to the renderer.
                auto const& frame = anim.frames[anim.currentFrame];
                sprite.uvMin = frame.uvMin;
                sprite.uvMax = frame.uvMax;
            }
        }

    private:
        ECS::Registry& m_registry;

    }; // class AnimationSystem

} // namespace Systems