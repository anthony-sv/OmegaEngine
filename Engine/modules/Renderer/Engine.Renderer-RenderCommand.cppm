export module Engine.Renderer:RenderCommand;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: RenderCommand
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Renderer
{

    // ─────────────────────────────────────────────────────────────────────
    // RenderCommand — thin static wrapper around OpenGL state changes.
    // Every raw glClear, glViewport, glDrawElements in the engine goes
    // through here. One file to change if the backend ever swaps to
    // Vulkan, DirectX, or Metal.
    // ─────────────────────────────────────────────────────────────────────

    export class RenderCommand
    {
    public:
        static void setClearColor(float r, float g, float b, float a);
        static void clear();
        static void setViewport(
            std::uint32_t x, std::uint32_t y,
            std::uint32_t width, std::uint32_t height
        );
        static void drawIndexed(std::uint32_t indexCount);

        RenderCommand() = delete;
	}; // class RenderCommand

} // namespace Renderer