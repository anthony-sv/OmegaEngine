module;

#include "glad/glad.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

module Engine.Renderer:Texture;

import :Texture;
import Engine.Core;
import std;

namespace Engine::Renderer
{

    using Core::ErrorInfo;
    using Core::ErrorCode;

    // Ω::Common GL setup ──────────────────────────────────────────────────
    // Both factories funnel through here: allocate immutable texture storage,
    // upload pixel data, and configure sampling parameters.

    void Texture2D::initStorage(
        std::uint32_t width, std::uint32_t height,
        void const* rgbaData
    )
    {
        m_width  = width;
        m_height = height;

        // DSA: create a named texture without binding it globally.
        glCreateTextures(GL_TEXTURE_2D, 1, &m_id);

        // Immutable storage: 1 mip level, 8 bits per RGBA channel.
        // glTextureStorage2D allocates once — can't resize later.
        glTextureStorage2D(m_id, 1, GL_RGBA8, m_width, m_height);

        // Upload the pixel data into the pre-allocated storage.
        glTextureSubImage2D(
            m_id, 0,
            0, 0, m_width, m_height,
            GL_RGBA, GL_UNSIGNED_BYTE,
            rgbaData
        );

        // Bilinear filtering: smooth when displayed at a different
        // resolution than the texture's native size.
        glTextureParameteri(m_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Repeat wrapping: UVs outside [0,1] tile the texture.
        glTextureParameteri(m_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    // Ω::Factory (file) ──────────────────────────────────────────────────

    Core::Result<Texture2D> Texture2D::create(std::filesystem::path const& path)
    {
        // OpenGL textures have origin at bottom-left; image files store
        // pixels top-to-bottom. Flip so UV (0,0) = bottom-left corner.
        stbi_set_flip_vertically_on_load(true);

        int width {}, height {}, channels {};
        // Force RGBA (4 channels) regardless of source format — simplifies
        // the GL upload path (always GL_RGBA + GL_RGBA8).
        auto* pixels = stbi_load(
            path.string().c_str(),
            &width, 
            &height, 
            &channels, 
            4
        );

        if (!pixels)
        {
            return std::unexpected(
                ErrorInfo::make(
                    ErrorCode::TextureLoadFailed,
                    std::format("failed to load '{}': {}",path.string(), stbi_failure_reason())
                )
            );
        }

        Texture2D tex;
        tex.initStorage(
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height),
            pixels
        );

        stbi_image_free(pixels);

        std::println(
            "[Ω::Texture2D] loaded {} ({}x{}, {} ch)",
            path.filename().string(), 
            width, 
            height, 
            channels
        );
        return tex;
    }

    // Ω::Factory (raw data) ──────────────────────────────────────────────

    Texture2D Texture2D::create(
        std::uint32_t width, 
        std::uint32_t height,
        void const* rgbaData
    )
    {
        Texture2D tex;
        tex.initStorage(width, height, rgbaData);
        return tex;
    }

    // Ω::Bind ────────────────────────────────────────────────────────────

    void Texture2D::bind(std::uint32_t slot) const
    {
        // DSA: binds this texture directly to a texture unit.
        // No glActiveTexture + glBindTexture dance needed.
        glBindTextureUnit(slot, m_id);
    }

    // Ω::RAII ────────────────────────────────────────────────────────────

    Texture2D::~Texture2D()
    {
        if (m_id) glDeleteTextures(1, &m_id);
    }

    Texture2D::Texture2D(Texture2D&& other) noexcept
        : m_id     { std::exchange(other.m_id, 0) }
        , m_width  { other.m_width }
        , m_height { other.m_height }
    {}

    Texture2D& Texture2D::operator=(Texture2D&& other) noexcept
    {
        if (this != &other)
        {
            if (m_id) glDeleteTextures(1, &m_id);
            m_id     = std::exchange(other.m_id, 0);
            m_width  = other.m_width;
            m_height = other.m_height;
        }
        return *this;
    }

} // namespace Renderer