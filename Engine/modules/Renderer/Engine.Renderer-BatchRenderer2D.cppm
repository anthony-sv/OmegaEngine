module;

#include "glm/glm.hpp"

export module Engine.Renderer:BatchRenderer2D;

import :Texture;
import :SubTexture2D;
import :Camera2D;
import Engine.Core;
import std;

/*===============================================================================
*
*          [[nodiscard]]
*       auto Render_Omega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      OMEGAENGINE :: Renderer2D
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
================================================================================*/

namespace Engine::Renderer
{

    // -----------------------------------------------------------------
    // Renderer2D -- static 2D batch rendering system.
    //
    // WHAT IS BATCH RENDERING?
    //
    //   Without batching, drawing 1000 quads means 1000 separate draw
    //   calls (glDrawElements). Each call has CPU overhead: driver
    //   validation, GPU state changes, command buffer submission. At
    //   ~1000+ draw calls per frame, the CPU becomes the bottleneck
    //   while the GPU sits idle waiting for commands.
    //
    //   Batch rendering solves this by accumulating geometry on the CPU
    //   into one big vertex buffer, then submitting it all in a SINGLE
    //   draw call. 1000 quads = 4000 vertices = 1 draw call. The CPU
    //   cost drops from O(n) draw calls to O(1).
    //
    // HOW IT WORKS (per frame):
    //
    //   1. beginBatch(camera)
    //      - Resets the CPU-side vertex write cursor to the start.
    //      - Uploads the camera's VP matrix to the batch shader.
    //
    //   2. drawQuad(...) / drawRotatedQuad(...)   [called many times]
    //      - Computes the 4 world-space corner positions on the CPU.
    //      - Writes 4 BatchVertex structs into the staging buffer.
    //      - If the batch is full (10k quads or 32 textures), it
    //        auto-flushes: uploads + draws what we have so far, then
    //        resets and keeps going. This is transparent to the caller.
    //
    //   3. endBatch()
    //      - Uploads the remaining vertices to the GPU (setSubData).
    //      - Binds all active textures to their slots.
    //      - Issues one glDrawElements call for all accumulated quads.
    //
    // -----------------------------------------------------------------

    export class Renderer2D
    {
    public:

        // ── Lifecycle ──────────────────────────────────────────────
        // Call init() once after the OpenGL context is ready.
        // It creates the internal shader, white texture, and GPU
        // buffers that the batch renderer uses every frame.
        [[nodiscard]] static Core::VoidResult init();

        // Call shutdown() before the OpenGL context is destroyed.
        // Releases all GPU resources the batch renderer owns.
        static void shutdown();

        // ── Frame scope ────────────────────────────────────────────
        // Every frame: beginBatch -> drawQuad (many) -> endBatch.
        // The camera's VP matrix is uploaded once in beginBatch.
        static void beginBatch(Camera2D const& camera);

        // Flushes the accumulated batch (one draw call), then resets.
        static void endBatch();

        // ── Axis-aligned quads ─────────────────────────────────────
        // "position" is the bottom-left corner in world space.
        // "size" is width x height in world units.

        // Color-only quad (uses the internal 1x1 white texture).
        static void drawQuad(
            glm::vec2 const& position,
            glm::vec2 const& size,
            glm::vec4 const& color
        );

        // Textured quad with optional tint color and UV tiling.
        static void drawQuad(
            glm::vec2 const& position,
            glm::vec2 const& size,
            Texture2D const& texture,
            glm::vec4 const& tintColor = glm::vec4(1.0f),
            float tilingFactor = 1.0f
        );

        // ── Sub-texture quads (texture atlas / sprite sheet) ───────
        // Same as the Texture2D overloads, but uses the UV sub-region
        // stored in the SubTexture2D instead of the full [0,1] range.
        // This is how you draw individual sprites from a sprite sheet.

        static void drawQuad(
            glm::vec2 const& position,
            glm::vec2 const& size,
            SubTexture2D const& subTexture,
            glm::vec4 const& tintColor = glm::vec4(1.0f),
            float tilingFactor = 1.0f
        );

        // ── Rotated quads ──────────────────────────────────────────
        // Same as above but rotated around the quad's center.
        // Rotation is in degrees, counter-clockwise positive.

        static void drawRotatedQuad(
            glm::vec2 const& position,
            glm::vec2 const& size,
            float rotationDegrees,
            glm::vec4 const& color
        );

        static void drawRotatedQuad(
            glm::vec2 const& position,
            glm::vec2 const& size,
            float rotationDegrees,
            Texture2D const& texture,
            glm::vec4 const& tintColor = glm::vec4(1.0f),
            float tilingFactor = 1.0f
        );

        static void drawRotatedQuad(
            glm::vec2 const& position,
            glm::vec2 const& size,
            float rotationDegrees,
            SubTexture2D const& subTexture,
            glm::vec4 const& tintColor = glm::vec4(1.0f),
            float tilingFactor = 1.0f
        );

        // ── Per-frame statistics ───────────────────────────────────
        // Useful for debugging: shows how many draw calls and quads
        // were submitted this frame. Reset at the start of each frame.
        struct Stats
        {
            std::uint32_t drawCalls { 0 };
            std::uint32_t quadCount { 0 };
        };

        [[nodiscard]] static Stats  stats();
        static void                 resetStats();

        // Static-only class -- no instances allowed.
        Renderer2D() = delete;

    }; // class Renderer2D

} // namespace Renderer