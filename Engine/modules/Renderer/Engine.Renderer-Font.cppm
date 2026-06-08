module;

#include "glm/glm.hpp"

export module Engine.Renderer:Font;

import :Texture;
import Engine.Core;
import std;

namespace Engine::Renderer
{

    // =================================================================
    //
    //  Font -- a TTF baked into a glyph atlas (via stb_truetype), laid
    //  out as textured quads for the batch renderer.
    //
    // =================================================================
    //
    // Units are EM: the bake's pixel height maps to 1.0 world unit, so a
    // caller scales the laid-out glyphs by a desired world text size. The
    // baked metrics live behind a PIMPL so stb_truetype stays in the .cpp.
    //
    // =================================================================

    export class Font
    {
    public:

        // Bake `ttf` at `pixelHeight` resolution. Error if the file can't be
        // read or the glyphs don't fit the atlas.
        [[nodiscard]] static Core::Result<Font> create(
            std::filesystem::path const& ttf, 
            float pixelHeight = 64.0f
        );

        // One laid-out glyph: quad corners in EM units (baseline at y = 0, y
        // UP) and its atlas UV sub-region. `cursor` is the pen X (EM); layout
        // advances it. Returns false for a blank/unsupported char (which still
        // advances the pen).
        struct Glyph
        {
            glm::vec2 min;     // quad bottom-left
            glm::vec2 max;     // quad top-right
            glm::vec2 uvMin;   // atlas UV bottom-left
            glm::vec2 uvMax;   // atlas UV top-right
        };

        [[nodiscard]] bool  layout(char c, float& cursor, Glyph& out) const;

        // Advance width of `text` in EM units (for centering / right-align).
        [[nodiscard]] float measure(std::string_view text) const;

        [[nodiscard]] Texture2D const& atlas() const { return *m_atlas; }

        Font(Font&&) noexcept;
        Font& operator=(Font&&) noexcept;
        Font(Font const&)            = delete;
        Font& operator=(Font const&) = delete;
        ~Font();

    private:
        Font() = default;

        struct Impl;
        std::unique_ptr<Impl>      m_impl; // baked glyph metrics (stb)
        std::unique_ptr<Texture2D> m_atlas;
        float                      m_pixelHeight { 64.0f };

    }; // class Font

} // namespace Renderer