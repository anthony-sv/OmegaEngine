export module Engine.Renderer:Texture;

import std;
import Engine.Core;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Texture
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
    // Texture2D — RAII wrapper around an OpenGL 2D texture.
    //
    // Two factories:
    //   create(path)            — loads PNG/JPG/BMP/TGA via stb_image.
    //                             Flips vertically for OpenGL's bottom-left
    //                             origin. Returns Result (can fail).
    //   create(w, h, rgbaData)  — from raw RGBA pixels. Cannot fail.
    //                             Used for the 1×1 white-pixel default.
    //
    // bind(slot) uses DSA (glBindTextureUnit) — no glActiveTexture needed.
    // The slot number matches the sampler uniform value in the shader:
    //   shader.setInt("u_Texture", 0);   // tell the shader "use slot 0"
    //   texture.bind(0);                 // put this texture in slot 0
    // ─────────────────────────────────────────────────────────────────────

    export class Texture2D
    {
    public:
        // Load from an image file. Forces RGBA (4 channels).
        [[nodiscard]] static Core::Result<Texture2D> create(std::filesystem::path const& path);

        // Create from raw RGBA pixel data (4 bytes per pixel).
        // Cannot fail — used for programmatic textures (white pixel, etc.).
        [[nodiscard]] static Texture2D create(
            std::uint32_t width, std::uint32_t height,
            void const* rgbaData
        );

        // Bind to a texture unit (0–31). Matches the sampler uniform slot.
        void bind(std::uint32_t slot = 0) const;

        [[nodiscard]] std::uint32_t id()     const { return m_id; }
        [[nodiscard]] std::uint32_t width()  const { return m_width; }
        [[nodiscard]] std::uint32_t height() const { return m_height; }

        ~Texture2D();
        Texture2D(Texture2D&& other) noexcept;
        Texture2D& operator=(Texture2D&& other) noexcept;
        Texture2D(Texture2D const&) = delete;
        Texture2D& operator=(Texture2D const&) = delete;

    private:
        Texture2D() = default;

        // Shared GL setup: allocate storage, upload pixels, set filtering.
        void initStorage(
            std::uint32_t width, std::uint32_t height,
            void const* rgbaData
        );

        std::uint32_t m_id     { 0 };
        std::uint32_t m_width  { 0 };
        std::uint32_t m_height { 0 };

    }; // class Texture2D

} // namespace Renderer