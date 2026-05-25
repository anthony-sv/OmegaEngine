module;

#include "glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

module Engine.Renderer:Shader;

import :Shader;
import Engine.Core;
import std;

namespace Engine::Renderer 
{

    using Core::ErrorInfo;
    using Core::ErrorCode;

    // ─────────────────────────────────────────────────────────────────────
    // Compiles one GLSL stage (vertex or fragment) into a GPU shader object.
    //
    // The pipeline:
    //   glCreateShader  — allocate an empty shader object on the GPU
    //   glShaderSource  — upload GLSL source text to driver memory
    //   glCompileShader — compile GLSL into GPU-native instructions
    //   glGetShaderiv   — query whether compilation succeeded
    //
    // On failure the driver provides an error log (line numbers, messages)
    // which we surface through Result.
    // ─────────────────────────────────────────────────────────────────────
    static Core::Result<GLuint> compileStage(GLenum type, std::string_view source) 
    {
        GLuint const shader = glCreateShader(type);

        auto const* data   = source.data();
        auto const  length = static_cast<GLint>(source.size());
        glShaderSource(shader, 1, &data, &length);
        glCompileShader(shader);

        GLint success {};
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success) 
        {
            GLint logLen {};
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);

            std::string log(static_cast<std::size_t>(logLen), '\0');
            glGetShaderInfoLog(shader, logLen, nullptr, log.data());
            glDeleteShader(shader);

            auto const* stage = (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
            return std::unexpected(
                ErrorInfo::make(
                    ErrorCode::ShaderCompileFailed,
                    std::format("{} shader compile error:\n{}", stage, log)
                )
            );
        }

        return shader;
    }

    // Ω::Factory ──────────────────────────────────────────────────────────

    Core::Result<Shader> Shader::fromSources(std::string_view vertexSource, std::string_view fragmentSource) 
    {
        auto vertResult = compileStage(GL_VERTEX_SHADER, vertexSource);
        if (!vertResult) return std::unexpected(vertResult.error());

        auto fragResult = compileStage(GL_FRAGMENT_SHADER, fragmentSource);
        if (!fragResult) 
        {
            glDeleteShader(*vertResult);
            return std::unexpected(fragResult.error());
        }

        // Linking combines compiled stages into one GPU-executable program.
        // The linker verifies that vertex shader outputs (out variables)
        // match fragment shader inputs (in variables) by name and type.
        GLuint const program = glCreateProgram();
        glAttachShader(program, *vertResult);
        glAttachShader(program, *fragResult);
        glLinkProgram(program);

        GLint success {};
        glGetProgramiv(program, GL_LINK_STATUS, &success);

        if (!success) 
        {
            GLint logLen {};
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);

            std::string log(static_cast<std::size_t>(logLen), '\0');
            glGetProgramInfoLog(program, logLen, nullptr, log.data());

            glDeleteProgram(program);
            glDeleteShader(*vertResult);
            glDeleteShader(*fragResult);

            return std::unexpected(
                    ErrorInfo::make(ErrorCode::ShaderLinkFailed, std::format("shader link error:\n{}", log)
                )
            );
        }

        // Individual shader objects can be freed after linking — the
        // program retains the compiled code internally.
        glDetachShader(program, *vertResult);
        glDetachShader(program, *fragResult);
        glDeleteShader(*vertResult);
        glDeleteShader(*fragResult);

        Shader shader;
        shader.m_program = program;

        std::println("[Ω::Shader] linked program {}", program);
        return shader;
    }

    // Ω::RAII ─────────────────────────────────────────────────────────────

    Shader::~Shader() 
    {
        if (m_program) glDeleteProgram(m_program);
    }

    Shader::Shader(Shader&& other) noexcept
        : m_program       { std::exchange(other.m_program, 0) }
        , m_locationCache { std::move(other.m_locationCache) }
    {}

    Shader& Shader::operator=(Shader&& other) noexcept 
    {
        if (this != &other) {
            if (m_program) glDeleteProgram(m_program);
            m_program       = std::exchange(other.m_program, 0);
            m_locationCache = std::move(other.m_locationCache);
        }
        return *this;
    }

    // Ω::State ────────────────────────────────────────────────────────────

    void Shader::bind()   const { glUseProgram(m_program); }
    void Shader::unbind() const { glUseProgram(0); }

    // Ω::Uniforms ─────────────────────────────────────────────────────────
    // glProgramUniform* (DSA, GL 4.1+) sets a uniform on a specific program
    // without requiring bind(). Legacy glUniform* operates on whichever
    // program is currently bound — DSA is explicit and less error-prone.

    void Shader::setInt(std::string_view name, int value) 
    {
        glProgramUniform1i(
            m_program, 
            location(name), 
            value
        );
    }

    void Shader::setFloat(std::string_view name, float value) 
    {
        glProgramUniform1f(
            m_program, 
            location(name), 
            value
        );
    }

    void Shader::setVec4(std::string_view name, glm::vec4 const& value) 
    {
        glProgramUniform4f(
            m_program, 
            location(name),
            value.x,
            value.y, 
            value.z, 
            value.w
        );
    }

    void Shader::setMat4(std::string_view name, glm::mat4 const& value) 
    {
        // GL_FALSE = don't transpose. GLM stores column-major, matching GL.
        glProgramUniformMatrix4fv(
            m_program, 
            location(name),
            1,
            GL_FALSE, 
            glm::value_ptr(value)
        );
    }

    void Shader::setIntArray(std::string_view name, std::span<int const> values) 
    {
        glProgramUniform1iv(
            m_program, 
            location(name),
            static_cast<GLsizei>(values.size()), 
            values.data()
        );
    }

    // Ω::Location cache ───────────────────────────────────────────────────

    std::int32_t Shader::location(std::string_view name) 
    {
        // glGetUniformLocation walks the program's symbol table — cheap but
        // not free. Cache results so repeated calls skip the driver query.
        // Heterogeneous find: string_view → no allocation on cache hits.
        if (auto const it = m_locationCache.find(name); it != m_locationCache.end())
            return it->second;

        std::string key { name };
        auto const loc = glGetUniformLocation(m_program, key.c_str());
        m_locationCache.emplace(std::move(key), loc);
        return loc;
    }

} // namespace Renderer