module;

#include "glad/glad.h"

module Engine.Renderer:VertexArray;

import :VertexArray;
import :Buffer;
import std;

namespace Engine::Renderer
{

    // ── GL type helpers (implementation detail) ─────────────────────────

    static constexpr GLenum shaderDataTypeToGLType(ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::Float:
            case ShaderDataType::Float2:
            case ShaderDataType::Float3:
            case ShaderDataType::Float4: return GL_FLOAT;
            case ShaderDataType::Int:
            case ShaderDataType::Int2:
            case ShaderDataType::Int3:
            case ShaderDataType::Int4:   return GL_INT;
            case ShaderDataType::Bool:   return GL_BOOL;
        }
        return 0;
    }

    static constexpr bool isIntegerType(ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::Int:
            case ShaderDataType::Int2:
            case ShaderDataType::Int3:
            case ShaderDataType::Int4:
            case ShaderDataType::Bool: return true;
            default:                   return false;
        }
    }

    // Ω::Factory ──────────────────────────────────────────────────────────

    VertexArray VertexArray::create()
    {
        VertexArray va;
        glCreateVertexArrays(1, &va.m_vao);
        return va;
    }

    // Ω::Buffer wiring ────────────────────────────────────────────────────
    // addVertexBuffer reads the VBO's BufferLayout and calls the DSA functions 
    //   glVertexArrayVertexBuffer   — bind VBO to a VAO binding index
    //   glVertexArrayAttribFormat   — describe one attribute (float types)
    //   glVertexArrayAttribIFormat  — describe one attribute (integer types)
    //   glVertexArrayAttribBinding  — wire attribute → binding index

    void VertexArray::addVertexBuffer(VertexBuffer const& vbo)
    {
        auto const& layout = vbo.layout();
        if (layout.empty())
        {
            std::println(std::cerr, "[Ω::VertexArray] cannot add VBO {} with empty layout", vbo.id());
            return;
        }

        // Bind the VBO to this VAO at the next available binding index.
        // Stride = total bytes per vertex, computed by the BufferLayout.
        glVertexArrayVertexBuffer(
            m_vao,
            m_nextBindingIndex,
            vbo.id(),
            0,
            static_cast<GLsizei>(layout.stride())
        );

        for (auto const& element : layout)
        {
            glEnableVertexArrayAttrib(m_vao, m_nextAttribIndex);

            auto const components = static_cast<GLint>(element.componentCount());
            auto const glType     = shaderDataTypeToGLType(element.type);

            if (isIntegerType(element.type))
            {
                // IFormat keeps values as integers in the shader (int, ivec2,
                // etc.). Regular AttribFormat silently converts to float —
                // a subtle bug if the shader expects int.
                glVertexArrayAttribIFormat(
                    m_vao, m_nextAttribIndex,
                    components, glType,
                    element.offset
                );
            }
            else
            {
                glVertexArrayAttribFormat(
                    m_vao, m_nextAttribIndex,
                    components, 
                    glType,
                    element.normalized ? GL_TRUE : GL_FALSE,
                    element.offset
                );
            }

            // Wire this attribute to the current binding index.
            // Multiple attributes sharing one VBO share the same binding.
            glVertexArrayAttribBinding(m_vao, m_nextAttribIndex, m_nextBindingIndex);
            m_nextAttribIndex++;
        }

        m_nextBindingIndex++;
    }

    void VertexArray::setIndexBuffer(IndexBuffer const& ibo)
    {
        glVertexArrayElementBuffer(m_vao, ibo.id());
        m_indexCount = ibo.count();
    }

    // Ω::State ────────────────────────────────────────────────────────────

    void VertexArray::bind()   const { glBindVertexArray(m_vao); }
    void VertexArray::unbind() const { glBindVertexArray(0); }

    // Ω::RAII ─────────────────────────────────────────────────────────────

    VertexArray::~VertexArray()
    {
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
    }

    VertexArray::VertexArray(VertexArray&& other) noexcept
        : m_vao              { std::exchange(other.m_vao, 0) }
        , m_indexCount       { other.m_indexCount }
        , m_nextAttribIndex  { other.m_nextAttribIndex }
        , m_nextBindingIndex { other.m_nextBindingIndex }
    {}

    VertexArray& VertexArray::operator=(VertexArray&& other) noexcept
    {
        if (this != &other)
        {
            if (m_vao) glDeleteVertexArrays(1, &m_vao);
            m_vao              = std::exchange(other.m_vao, 0);
            m_indexCount       = other.m_indexCount;
            m_nextAttribIndex  = other.m_nextAttribIndex;
            m_nextBindingIndex = other.m_nextBindingIndex;
        }
        return *this;
    }

} // namespace Renderer