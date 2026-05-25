module;

#include "glm/glm.hpp"

export module Engine.Renderer:Shader;

import std;
import Engine.Core;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Shader
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
    // A Shader wraps an OpenGL "program" — the linked combination of a
    // vertex shader (runs per-vertex, positions geometry) and a fragment
    // shader (runs per-pixel, determines color). The GPU executes exactly
    // one program at a time; bind() makes this one active.
    // ─────────────────────────────────────────────────────────────────────

    export class Shader 
    {
    public:
        [[nodiscard]] static Engine::Core::Result<Shader> fromSources(
            std::string_view vertexSource,
            std::string_view fragmentSource
        );

        ~Shader();
        Shader(Shader&& other) noexcept;
        Shader& operator=(Shader&& other) noexcept;
        Shader(Shader const&) = delete;
        Shader& operator=(Shader const&) = delete;

        void bind()   const;
        void unbind() const;

        // Ω::Uniforms ────────────────────────────────────────────────
        // CPU → GPU per-draw parameters. Unlike vertex data (varies per
        // vertex), a uniform is constant across an entire draw call —
        // camera matrix, tint color, texture slot index, etc.
        // These use glProgramUniform* (DSA) so bind() is NOT required.
        void setInt(std::string_view name, int value);
        void setFloat(std::string_view name, float value);
        void setVec4(std::string_view name, glm::vec4 const& value);
        void setMat4(std::string_view name, glm::mat4 const& value);
        void setIntArray(std::string_view name, std::span<int const> values);

        [[nodiscard]] std::uint32_t id() const { return m_program; }

    private:
        Shader() = default;

        [[nodiscard]] std::int32_t location(std::string_view name);

        std::uint32_t m_program { 0 };
        // flat_map: sorted contiguous storage. A shader has ~5–15 uniforms,
        // so binary search in a cache line beats hash-table indirection.
        // std::less<> enables heterogeneous lookup (find by string_view
        // without constructing a std::string on every call).
        std::flat_map<std::string, std::int32_t, std::less<>> m_locationCache;

    }; // class Shader

} // namespace Renderer