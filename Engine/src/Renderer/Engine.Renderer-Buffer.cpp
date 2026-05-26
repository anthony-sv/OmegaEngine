module;

#include "glad/glad.h"

module Engine.Renderer:Buffer;

import :Buffer;
import std;

namespace Engine::Renderer
{

    // Ω::BufferElement ────────────────────────────────────────────────────

    BufferElement::BufferElement(
        ShaderDataType type,
        std::string name,
        bool normalized
    )
        : type       { type }
        , name       { std::move(name) }
        , size       { shaderDataTypeSize(type) }
        , normalized { normalized }
    {}

    // Ω::BufferLayout ────────────────────────────────────────────────────

    BufferLayout::BufferLayout(std::initializer_list<BufferElement> elements)
        : m_elements{ elements }
    {
        calculateOffsetsAndStride();
    }

    void BufferLayout::calculateOffsetsAndStride()
    {
        // Walk the elements in order, accumulating byte offsets.
        // The stride is the total size of one vertex (sum of all elements).
        std::uint32_t offset = 0;
        for (auto& element : m_elements)
        {
            element.offset = offset;
            offset += element.size;
        }
        m_stride = offset;
    }

    // Ω::VertexBuffer ────────────────────────────────────────────────────

    VertexBuffer VertexBuffer::create(void const* data, std::uint32_t sizeBytes)
    {
        VertexBuffer vb;
        glCreateBuffers(1, &vb.m_id);
        // Immutable storage: ideal for static geometry that never changes.
        // Flags = 0 means "GPU-only" — no client read/write access needed.
        glNamedBufferStorage(vb.m_id, sizeBytes, data, 0);
        return vb;
    }

    VertexBuffer VertexBuffer::createDynamic(std::uint32_t sizeBytes)
    {
        VertexBuffer vb;
        glCreateBuffers(1, &vb.m_id);
        // GL_DYNAMIC_STORAGE_BIT: allows updates via glNamedBufferSubData.
        // nullptr = allocate without initializing (filled per-frame by the
        // batch renderer).
        glNamedBufferStorage(vb.m_id, sizeBytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
        return vb;
    }

    void VertexBuffer::setSubData(void const* data, std::uint32_t sizeBytes)
    {
        // DSA: updates by object ID, no bind needed.
        glNamedBufferSubData(m_id, 0, sizeBytes, data);
    }

    void VertexBuffer::setLayout(BufferLayout layout)
    {
        m_layout = std::move(layout);
    }

    // Ω::VertexBuffer RAII ───────────────────────────────────────────────

    VertexBuffer::~VertexBuffer()
    {
        if (m_id) glDeleteBuffers(1, &m_id);
    }

    VertexBuffer::VertexBuffer(VertexBuffer&& other) noexcept
        : m_id     { std::exchange(other.m_id, 0) }
        , m_layout { std::move(other.m_layout) }
    {}

    VertexBuffer& VertexBuffer::operator=(VertexBuffer&& other) noexcept
    {
        if (this != &other)
        {
            if (m_id) glDeleteBuffers(1, &m_id);
            m_id     = std::exchange(other.m_id, 0);
            m_layout = std::move(other.m_layout);
        }
        return *this;
    }

    // Ω::IndexBuffer ─────────────────────────────────────────────────────

    IndexBuffer IndexBuffer::create(std::span<std::uint32_t const> indices)
    {
        IndexBuffer ib;
        ib.m_count = static_cast<std::uint32_t>(indices.size());
        glCreateBuffers(1, &ib.m_id);
        glNamedBufferStorage(
            ib.m_id,
            static_cast<GLsizeiptr>(indices.size_bytes()),
            indices.data(),
            0
        );
        return ib;
    }

    // Ω::IndexBuffer RAII ────────────────────────────────────────────────

    IndexBuffer::~IndexBuffer()
    {
        if (m_id) glDeleteBuffers(1, &m_id);
    }

    IndexBuffer::IndexBuffer(IndexBuffer&& other) noexcept
        : m_id    { std::exchange(other.m_id, 0) }
        , m_count { other.m_count }
    {}

    IndexBuffer& IndexBuffer::operator=(IndexBuffer&& other) noexcept
    {
        if (this != &other)
        {
            if (m_id) glDeleteBuffers(1, &m_id);
            m_id    = std::exchange(other.m_id, 0);
            m_count = other.m_count;
        }
        return *this;
    }

} // namespace Renderer