module;

#include "glm/glm.hpp"

export module Engine.ECS:AnimationComponents;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Animation
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::ECS
{

    // -----------------------------------------------------------------
    // SpriteAnimation -- frame-based sprite animation.
    //
    // Stores a sequence of UV rectangles (frames) that an animation
    // system cycles through over time. Each frame is a sub-region
    // of the sprite sheet set in SpriteRenderer::texture.
    //
    // HOW IT WORKS (with an animation system):
    //
    //   1. Each tick, the system adds dt to "elapsed".
    //   2. When elapsed >= frameDuration, it advances currentFrame.
    //   3. If looping, wraps back to 0 at the end. Otherwise stops.
    //   4. The render system reads frames[currentFrame].uvMin/uvMax
    //      and overrides SpriteRenderer's UV coords for that frame.
    //
    // The texture itself is NOT stored here -- it lives on the
    // SpriteRenderer component. SpriteAnimation only controls
    // WHICH PART of the texture is shown, not which texture.
    //
    // Example: a 4-frame walk cycle on a horizontal sprite strip
    //
    //   SpriteAnimation anim;
    //   anim.frameDuration = 0.12f;    // ~8 FPS
    //   anim.frames = {
    //       { {0.00f, 0.0f}, {0.25f, 1.0f} },   // frame 0
    //       { {0.25f, 0.0f}, {0.50f, 1.0f} },   // frame 1
    //       { {0.50f, 0.0f}, {0.75f, 1.0f} },   // frame 2
    //       { {0.75f, 0.0f}, {1.00f, 1.0f} },   // frame 3
    //   };
    //   entity.add<SpriteAnimation>(std::move(anim));
    // -----------------------------------------------------------------

    export struct SpriteAnimation
    {
        // One frame = a UV sub-region of the sprite sheet.
        struct Frame
        {
            glm::vec2 uvMin { 0.0f, 0.0f };
            glm::vec2 uvMax { 1.0f, 1.0f };
        };

        std::vector<Frame> frames {};

        float         frameDuration { 0.1f };       // seconds per frame
        float         elapsed       { 0.0f };       // time accumulator
        std::uint32_t currentFrame  { 0 };          // index into frames

        bool looping { true };
        bool playing { true };
	}; // struct SpriteAnimation

} // namespace ECS