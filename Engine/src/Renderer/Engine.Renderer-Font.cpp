module;

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

module Engine.Renderer:Font;

import :Font;
import :Texture;
import Engine.Core;
import std;

namespace Engine::Renderer
{
    using Core::ErrorInfo;
    using Core::ErrorCode;

    namespace
    {
        constexpr int First  = 32;     // first printable ASCII (space)
        constexpr int Count  = 95;     // 32..126 inclusive
        constexpr int AtlasW = 1024;
        constexpr int AtlasH = 1024;
    }

    struct Font::Impl
    {
        std::array<stbtt_bakedchar, Count> chars {};
    };

    Font::Font(Font&&) noexcept            = default;
    Font& Font::operator=(Font&&) noexcept = default;
    Font::~Font()                          = default;

    Core::Result<Font> Font::create(std::filesystem::path const& ttf, float pixelHeight)
    {
        std::ifstream file(ttf, std::ios::binary);
        if (!file)
            return std::unexpected(
                ErrorInfo::make(
                    ErrorCode::TextureLoadFailed,
                    std::format("failed to open font '{}'", ttf.string())
                )
            );

        std::vector<unsigned char> const data(
            (std::istreambuf_iterator<char>(file)), 
            std::istreambuf_iterator<char>()
        );

        auto impl = std::make_unique<Impl>();

        // Bake the printable ASCII range into a single-channel coverage bitmap.
        std::vector<unsigned char> alpha(static_cast<std::size_t>(AtlasW) * AtlasH);
        int const rows = ::stbtt_BakeFontBitmap(
            data.data(), 
            0, 
            pixelHeight, 
            alpha.data(), 
            AtlasW, 
            AtlasH, 
            First, 
            Count, 
            impl->chars.data()
        );
        if (rows <= 0)
            return std::unexpected(
                ErrorInfo::make(
                    ErrorCode::TextureLoadFailed,
                    std::format("failed to bake font '{}' (atlas too small)", ttf.string())
                )
            );

        // Expand coverage -> RGBA (white, alpha = coverage); the renderer tints
        // by the text colour and alpha-blends the glyph shape.
        std::vector<unsigned char> rgba(static_cast<std::size_t>(AtlasW) * AtlasH * 4);
        for (std::size_t i = 0; i < alpha.size(); ++i)
        {
            rgba[i * 4 + 0] = 255;
            rgba[i * 4 + 1] = 255;
            rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = alpha[i];
        }

        Font font;
        font.m_impl        = std::move(impl);
        font.m_pixelHeight = pixelHeight;
        font.m_atlas       = std::make_unique<Texture2D>(
            Texture2D::create(
                AtlasW, AtlasH, 
                rgba.data(), 
                TextureFilter::Linear
            )
        );
        return font;
    }

    bool Font::layout(char c, float& cursor, Glyph& out) const
    {
        if (c < First || c >= First + Count)
            return false;

        float x = cursor;
        float y = 0.0f;
        stbtt_aligned_quad q;
        ::stbtt_GetBakedQuad(m_impl->chars.data(), AtlasW, AtlasH, c - First, &x, &y, &q, 1);
        cursor = x;     // pen advanced (pixels)

        // stb gives q.x0,y0 (top-left) / q.x1,y1 (bottom-right) in pixels with y
        // DOWN; our world is y UP, so negate y. The atlas is uploaded WITHOUT a
        // vertical flip, so the glyph's bitmap-top (t0) lands at GL v = t0 --
        // map the quad bottom to t1 and the top to t0.
        float const inv = 1.0f / m_pixelHeight;
        out.min   = { q.x0 * inv, -q.y1 * inv };
        out.max   = { q.x1 * inv, -q.y0 * inv };
        out.uvMin = { q.s0, q.t1 };
        out.uvMax = { q.s1, q.t0 };
        return c != ' '; // a space advances the pen but draws nothing
    }

    float Font::measure(std::string_view text) const
    {
        float cursor = 0.0f;
        float y      = 0.0f;
        for (char const c : text)
        {
            if (c < First || c >= First + Count)
                continue;
            stbtt_aligned_quad q;
            ::stbtt_GetBakedQuad(m_impl->chars.data(), AtlasW, AtlasH, c - First, &cursor, &y, &q, 1);
        }
        return cursor / m_pixelHeight;
    }

} // namespace Renderer