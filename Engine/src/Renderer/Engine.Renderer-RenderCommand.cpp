module;

#include "glad/glad.h"

module Engine.Renderer:RenderCommand;

import :RenderCommand;
import std;

namespace Engine::Renderer
{

    void RenderCommand::setClearColor(float r, float g, float b, float a)
    {
        glClearColor(r, g, b, a);
    }

    void RenderCommand::clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void RenderCommand::setViewport(
        std::uint32_t x, std::uint32_t y,
        std::uint32_t width, std::uint32_t height)
    {
        glViewport(
            static_cast<GLint>(x),
            static_cast<GLint>(y),
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height)
        );
    }

    void RenderCommand::drawIndexed(std::uint32_t indexCount)
    {
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(indexCount),
            GL_UNSIGNED_INT,
            nullptr
        );
    }

} // namespace Renderer