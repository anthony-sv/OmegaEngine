export module Engine.Renderer:VertexArray;

import :Buffer;
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: VertexArray
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
    // VertexArray — wraps an OpenGL VAO (Vertex Array Object).
    // A VAO captures the vertex format: which buffers to read, how many
    // attributes, their types, offsets, and strides. Binding a VAO
    // restores all of this state in a single call — no need to re-specify
    // the layout every frame.
    //
    // Non-owning: the VertexBuffer(s) and IndexBuffer wired via
    // addVertexBuffer / setIndexBuffer must outlive this object.
    // ─────────────────────────────────────────────────────────────────────

    export class VertexArray
    {
    public:
        [[nodiscard]] static VertexArray create();

        // Wire a VBO to this VAO. Reads vbo.layout() to configure
        // vertex attributes (count, type, offset, stride). The layout
        // must be set on the VBO before calling this.
        void addVertexBuffer(VertexBuffer const& vbo);

        // Wire an EBO to this VAO and store its index count.
        void setIndexBuffer(IndexBuffer const& ibo);

        void bind()   const;
        void unbind() const;

        [[nodiscard]] std::uint32_t indexCount() const { return m_indexCount; }
        [[nodiscard]] std::uint32_t id()         const { return m_vao; }

        ~VertexArray();
        VertexArray(VertexArray&& other) noexcept;
        VertexArray& operator=(VertexArray&& other) noexcept;
        VertexArray(VertexArray const&) = delete;
        VertexArray& operator=(VertexArray const&) = delete;

    private:
        VertexArray() = default;

        std::uint32_t m_vao              { 0 };
        std::uint32_t m_indexCount       { 0 };
        std::uint32_t m_nextAttribIndex  { 0 };
        std::uint32_t m_nextBindingIndex { 0 };
	}; // class VertexArray

} // namespace Renderer