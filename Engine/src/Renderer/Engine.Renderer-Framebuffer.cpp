module;

#include "glad/glad.h"

module Engine.Renderer:Framebuffer;

import :Framebuffer;
import Engine.Core;
import std;

namespace Engine::Renderer 
{

    using Core::ErrorInfo;
    using Core::ErrorCode;

    // Ω::Attachments ──────────────────────────────────────────────────────

    void Framebuffer::createAttachments()
    {
        // Color attachment: an RGBA texture the GPU writes pixels into.
        // This texture is what ImGui::Image displays in the Viewport panel.
        // GL_RGBA8 = 8 bits per channel, 4 channels = 32 bits per pixel.
        glCreateTextures(GL_TEXTURE_2D, 1, &m_colorTexture);
        glTextureStorage2D(m_colorTexture, 1, GL_RGBA8, m_width, m_height);

        // GL_LINEAR for viewport display (smooth scaling when the panel is
        // a different size than the FBO). Pixel-art GL_NEAREST is for game
        // textures, not for the viewport framebuffer.
        glTextureParameteri(m_colorTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_colorTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Wire the texture to the FBO's color output slot 0.
        // When a fragment shader writes to layout(location = 0), pixels
        // land in this texture.
        glNamedFramebufferTexture(m_fbo, GL_COLOR_ATTACHMENT0, m_colorTexture, 0);
    }

    void Framebuffer::deleteAttachments()
    {
        if (m_colorTexture) 
        {
            glDeleteTextures(1, &m_colorTexture);
            m_colorTexture = 0;
        }
    }

    // Ω::Factory ──────────────────────────────────────────────────────────

    Core::Result<Framebuffer> Framebuffer::create(
        std::uint32_t width,
        std::uint32_t height)
    {
        Framebuffer fb;

        // glCreateFramebuffers (DSA) — allocates an FBO without binding it.
        // An FBO is just a collection of attachment points (color, depth,
        // stencil). It doesn't own pixel storage — the attached textures do.
        glCreateFramebuffers(1, &fb.m_fbo);
        fb.m_width  = width;
        fb.m_height = height;
        fb.createAttachments();

        // An FBO must be "complete" before rendering into it: all attachments
        // must be allocated, have compatible formats, and share the same
        // sample count. The driver returns what's wrong if it fails.
        auto const status = glCheckNamedFramebufferStatus(fb.m_fbo, GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) 
        {
            return std::unexpected(
                ErrorInfo::make(
                    ErrorCode::FramebufferCreationFailed,
                    std::format("framebuffer incomplete (status 0x{:X})", status)
                )
            );
        }

        std::println("[Ω::Framebuffer] created {}x{} (FBO={}, color={})",
            width, height, fb.m_fbo, fb.m_colorTexture);
        return fb;
    }

    // Ω::RAII ─────────────────────────────────────────────────────────────

    Framebuffer::~Framebuffer()
    {
        deleteAttachments();
        if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    }

    Framebuffer::Framebuffer(Framebuffer&& other) noexcept
        : m_fbo          { std::exchange(other.m_fbo, 0) }
        , m_colorTexture { std::exchange(other.m_colorTexture, 0) }
        , m_width        { other.m_width }
        , m_height       { other.m_height }
    {}

    Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
    {
        if (this != &other) 
        {
            deleteAttachments();
            if (m_fbo) glDeleteFramebuffers(1, &m_fbo);

            m_fbo          = std::exchange(other.m_fbo, 0);
            m_colorTexture = std::exchange(other.m_colorTexture, 0);
            m_width        = other.m_width;
            m_height       = other.m_height;
        }
        return *this;
    }

    // Ω::Bind/Unbind ──────────────────────────────────────────────────────

    void Framebuffer::bind() const
    {
        // Save the current viewport so unbind() can restore it.
        // glViewport defines which rectangle of the framebuffer receives
        // rendered output — it MUST match the FBO's dimensions, otherwise
        // the scene renders at the wrong resolution or gets clipped.
        glGetIntegerv(GL_VIEWPORT, m_savedViewport.data());
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, m_width, m_height);
    }

    void Framebuffer::unbind() const
    {
        // Return to the default framebuffer (the window) and restore the
        // viewport that was active before bind().
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(
            m_savedViewport[0], m_savedViewport[1],
            m_savedViewport[2], m_savedViewport[3]
        );
    }

    // Ω::Resize ───────────────────────────────────────────────────────────

    void Framebuffer::resize(std::uint32_t width, std::uint32_t height)
    {
        if (width == 0 || height == 0) return;
        if (width == m_width && height == m_height) return;

        m_width  = width;
        m_height = height;

        // glTextureStorage2D allocates immutable storage — can't resize in
        // place. Delete the old texture and create a fresh one. The FBO
        // object is reused; only its color attachment changes.
        deleteAttachments();
        createAttachments();
    }

} // namespace Renderer