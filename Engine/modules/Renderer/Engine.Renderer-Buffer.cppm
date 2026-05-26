export module Engine.Renderer:Buffer;

import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Buffer
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
    // ShaderDataType — vertex attribute data types. Maps to GL types
    // (GL_FLOAT, GL_INT, etc.) in the implementation, but no OpenGL
    // headers leak into the public API.
    // ─────────────────────────────────────────────────────────────────────

    export enum class ShaderDataType : std::uint8_t
    {
        Float,
        Float2, 
        Float3, 
        Float4,
        Int,   
        Int2,   
        Int3,   
        Int4,
        Bool,
	}; // enum class ShaderDataType

    // Size in bytes of one instance of the given type.
    export [[nodiscard]] constexpr std::uint32_t shaderDataTypeSize(ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::Float:  return 4;
            case ShaderDataType::Float2: return 4 * 2;
            case ShaderDataType::Float3: return 4 * 3;
            case ShaderDataType::Float4: return 4 * 4;
            case ShaderDataType::Int:    return 4;
            case ShaderDataType::Int2:   return 4 * 2;
            case ShaderDataType::Int3:   return 4 * 3;
            case ShaderDataType::Int4:   return 4 * 4;
            case ShaderDataType::Bool:   return 1;
        }
        return 0;
    }

    // Number of scalar components (e.g., Float3 = 3, Int2 = 2).
    export [[nodiscard]] constexpr std::uint32_t shaderDataTypeComponentCount(ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::Float:  return 1;
            case ShaderDataType::Float2: return 2;
            case ShaderDataType::Float3: return 3;
            case ShaderDataType::Float4: return 4;
            case ShaderDataType::Int:    return 1;
            case ShaderDataType::Int2:   return 2;
            case ShaderDataType::Int3:   return 3;
            case ShaderDataType::Int4:   return 4;
            case ShaderDataType::Bool:   return 1;
        }
        return 0;
    }

    // ─────────────────────────────────────────────────────────────────────
    // BufferElement / BufferLayout — declarative vertex attribute
    // description. Instead of manually calling glVertexArrayAttribFormat
    // for each attribute, you declare the layout once:
    //
    //   BufferLayout layout = {
    //       { ShaderDataType::Float2, "a_Position" },
    //       { ShaderDataType::Float3, "a_Color" },
    //   };
    //   // stride = 20 bytes, offsets = [0, 8] — computed automatically.
    // ─────────────────────────────────────────────────────────────────────

    export struct BufferElement
    {
        ShaderDataType type;
        std::string    name;
        std::uint32_t  offset     { 0 };
        std::uint32_t  size       { 0 };
        bool           normalized { false };

        BufferElement(ShaderDataType type, std::string name, bool normalized = false);

        [[nodiscard]] constexpr std::uint32_t componentCount() const
        {
            return shaderDataTypeComponentCount(type);
        }
	}; // struct BufferElement

    export class BufferLayout
    {
    public:
        BufferLayout() = default;
        BufferLayout(std::initializer_list<BufferElement> elements);

        [[nodiscard]] auto const& elements()  const { return m_elements; }
        [[nodiscard]] std::uint32_t stride()  const { return m_stride; }
        [[nodiscard]] bool empty()            const { return m_elements.empty(); }

        auto begin() const { return m_elements.begin(); }
        auto end()   const { return m_elements.end(); }

    private:
        void calculateOffsetsAndStride();

        std::vector<BufferElement> m_elements;
        std::uint32_t m_stride { 0 };
	}; // class BufferLayout

    // ─────────────────────────────────────────────────────────────────────
    // VertexBuffer — GPU-side vertex storage (VBO).
    // RAII wrapper around an OpenGL buffer object. Supports:
    //   Static:  immutable storage for fixed geometry (test quad, meshes).
    //   Dynamic: mutable storage for per-frame updates (batch renderer).
    //
    // The BufferLayout must be set before passing to a VertexArray —
    // it describes how the GPU interprets each vertex's byte pattern.
    //
    // Buffer creation is a trivial GPU allocation that cannot
    // meaningfully fail (unlike shader compilation), so factories
    // return the object directly instead of Result<T>.
    // ─────────────────────────────────────────────────────────────────────

    export class VertexBuffer
    {
    public:
        // Immutable storage — data provided upfront, cannot be modified.
        [[nodiscard]] static VertexBuffer create(void const* data, std::uint32_t sizeBytes);

        // Mutable storage — allocated empty, filled per-frame via setSubData.
        [[nodiscard]] static VertexBuffer createDynamic(
            std::uint32_t sizeBytes);

        // Upload new data into a dynamic buffer (offset always 0).
        void setSubData(void const* data, std::uint32_t sizeBytes);

        void setLayout(BufferLayout layout);
        [[nodiscard]] BufferLayout const& layout() const { return m_layout; }
        [[nodiscard]] std::uint32_t id()           const { return m_id; }

        ~VertexBuffer();
        VertexBuffer(VertexBuffer&& other) noexcept;
        VertexBuffer& operator=(VertexBuffer&& other) noexcept;
        VertexBuffer(VertexBuffer const&) = delete;
        VertexBuffer& operator=(VertexBuffer const&) = delete;

    private:
        VertexBuffer() = default;

        std::uint32_t m_id { 0 };
        BufferLayout  m_layout;
	}; // class VertexBuffer

    // ─────────────────────────────────────────────────────────────────────
    // IndexBuffer — GPU-side index storage (EBO).
    // Stores vertex indices that glDrawElements reads. Enables vertex
    // reuse: a quad is 4 vertices + 6 indices instead of 6 duplicated
    // vertices. At batch scale (10k quads): 40k vs 60k vertices.
    // ─────────────────────────────────────────────────────────────────────

    export class IndexBuffer
    {
    public:
        [[nodiscard]] static IndexBuffer create(std::span<std::uint32_t const> indices);

        [[nodiscard]] std::uint32_t id()    const { return m_id; }
        [[nodiscard]] std::uint32_t count() const { return m_count; }

        ~IndexBuffer();
        IndexBuffer(IndexBuffer&& other) noexcept;
        IndexBuffer& operator=(IndexBuffer&& other) noexcept;
        IndexBuffer(IndexBuffer const&) = delete;
        IndexBuffer& operator=(IndexBuffer const&) = delete;

    private:
        IndexBuffer() = default;

        std::uint32_t m_id    { 0 };
        std::uint32_t m_count { 0 };
	}; // class IndexBuffer

} // namespace Renderer