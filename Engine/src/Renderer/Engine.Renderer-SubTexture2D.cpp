module;

#include "glm/glm.hpp"

module Engine.Renderer:SubTexture2D;

import :SubTexture2D;
import :Texture;
import std;

namespace Engine::Renderer
{

    // Omega::Constructor (raw UVs) -------------------------------------------

    SubTexture2D::SubTexture2D(
        Texture2D const& atlas,
        glm::vec2 const& uvMin,
        glm::vec2 const& uvMax
    )
        : m_atlas { &atlas }
        , m_uvMin { uvMin }
        , m_uvMax { uvMax }
    {}

    // Omega::Factory (grid) --------------------------------------------------
    //
    // Converts (column, row) grid coordinates into OpenGL UV coordinates.
    //
    // The tricky part is the Y axis. In the image file (PNG/JPG), row 0
    // is the TOP of the image -- that's how you see it in an image editor.
    // But in OpenGL's UV space, Y=0 is the BOTTOM of the texture.
    //
    // stb_image flips the pixels on load (stbi_set_flip_vertically_on_load),
    // so OpenGL renders the image right-side up. But UVs still follow
    // OpenGL convention: Y=0 = bottom, Y=1 = top.
    //
    // To convert from "image-space row" to UV:
    //
    //   Image row 0 (top of image)    --> UV y close to 1.0
    //   Image row N (bottom of image) --> UV y close to 0.0
    //
    // Formula:
    //   uvMin.y = 1.0 - (row + spriteH) * cellH / textureH   (bottom edge)
    //   uvMax.y = 1.0 - row * cellH / textureH               (top edge)
    //
    // This ensures uvMin.y < uvMax.y (bottom < top in UV space).

    SubTexture2D SubTexture2D::createFromGrid(
        Texture2D const& atlas,
        glm::vec2 const& coords,
        glm::vec2 const& cellSize,
        glm::vec2 const& spriteSize
    )
    {
        auto const texW = static_cast<float>(atlas.width());
        auto const texH = static_cast<float>(atlas.height());

        // X axis: straightforward -- left-to-right in both image and UV.
        float const uvMinX = (coords.x * cellSize.x) / texW;
        float const uvMaxX = ((coords.x + spriteSize.x) * cellSize.x) / texW;

        // Y axis: flip from image-space (top-down) to UV-space (bottom-up).
        // "coords.y" is the row counting from the top of the image.
        float const uvMinY = 1.0f - ((coords.y + spriteSize.y) * cellSize.y) / texH;
        float const uvMaxY = 1.0f - (coords.y * cellSize.y) / texH;

        return SubTexture2D { atlas, { uvMinX, uvMinY }, { uvMaxX, uvMaxY } };
    }

} // namespace Renderer