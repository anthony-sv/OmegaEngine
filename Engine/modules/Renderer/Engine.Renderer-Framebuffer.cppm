export module Engine.Renderer:Framebuffer;

import std;
import Engine.Core;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Framebuffer
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::Renderer {

    // ─────────────────────────────────────────────────────────────────────
    // A Framebuffer wraps an OpenGL FBO — a "virtual screen" the GPU can
    // render into instead of the real window. The result is stored as a
    // texture (colorAttachment) that can be displayed anywhere — here,
    // inside an ImGui panel as the game viewport.
    //
    // bind() redirects all draw calls into this FBO's color attachment.
    // unbind() returns to the default framebuffer (the window) and
    // restores the previous viewport dimensions.
    // ─────────────────────────────────────────────────────────────────────

    export class Framebuffer {
    public:
        [[nodiscard]] static Core::Result<Framebuffer> create(
            std::uint32_t width,
            std::uint32_t height
        );

        ~Framebuffer();
        Framebuffer(Framebuffer&& other) noexcept;
        Framebuffer& operator=(Framebuffer&& other) noexcept;
        Framebuffer(Framebuffer const&) = delete;
        Framebuffer& operator=(Framebuffer const&) = delete;

        void bind()   const;
        void unbind() const;

        void resize(std::uint32_t width, std::uint32_t height);

        [[nodiscard]] std::uint32_t colorAttachment() const { return m_colorTexture; }
        [[nodiscard]] std::uint32_t width()           const { return m_width; }
        [[nodiscard]] std::uint32_t height()          const { return m_height; }

    private:
        Framebuffer() = default;

        void createAttachments();
        void deleteAttachments();

        std::uint32_t m_fbo          { 0 };
        std::uint32_t m_colorTexture { 0 };
        std::uint32_t m_width        { 0 };
        std::uint32_t m_height       { 0 };

        // Saved by bind(), restored by unbind(). Mutable because binding
        // changes GL state, not the logical state of this object.
        mutable std::array<std::int32_t, 4> m_savedViewport {};

    }; // class Framebuffer

} // namespace Renderer