export module Engine.Renderer:Screenshot;

import Engine.Core;   // Core::VoidResult
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Screenshot
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Renderer
{

    // =================================================================
    //
    //  Screenshot -- capture a framebuffer region to a PNG.
    //
    // =================================================================
    //
    // Reads pixels from the CURRENTLY BOUND read-framebuffer (the window's
    // default framebuffer, or an FBO the caller has bound) and writes a
    // PNG. The image is flipped vertically because OpenGL's origin is
    // bottom-left while PNG is top-down. Parent directories are created.
    //
    // Call it AFTER drawing the frame and (for the default framebuffer)
    // BEFORE swapBuffers, so the back buffer still holds the frame.
    //
    // =================================================================

    export class Screenshot
    {
    public:

        // Capture [x, y, width, height] of the bound framebuffer to `path`.
        [[nodiscard]] static Core::VoidResult capture(
            std::filesystem::path const& path,
            int x, int y, 
            int width, int height
        );

        // Build a unique, sortable path: <dir>/<prefix>_YYYYMMDD_HHMMSS.png
        // (UTC). Done with integer formatting -- no locale facets -- so it
        // is safe to call from any translation unit.
        [[nodiscard]] static std::filesystem::path timestamped(
            std::string_view dir = "screenshots", 
            std::string_view prefix = "shot"
        );

        Screenshot() = delete;

    }; // class Screenshot

} // namespace Renderer