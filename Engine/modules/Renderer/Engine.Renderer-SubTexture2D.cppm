module;

#include "glm/glm.hpp"

export module Engine.Renderer:SubTexture2D;

import :Texture;
import std;

/*===============================================================================
*
*          [[nodiscard]]
*       auto Render_Omega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      OMEGAENGINE :: SubTexture2D
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
================================================================================*/

namespace Engine::Renderer
{

    // -----------------------------------------------------------------
    // SubTexture2D -- a rectangular sub-region of a larger texture.
    //
    // WHY?
    //
    //   The batch renderer can bind up to 32 textures per batch. Once
    //   all slots are full, it must flush (a new draw call). If your
    //   game has 100 different sprites each in its own image file,
    //   you'd burn through those 32 slots fast, causing many draw
    //   calls per frame.
    //
    //   The solution: pack all sprites into ONE big texture (a "texture
    //   atlas" or "sprite sheet"). Now 100 sprites share a single
    //   texture slot. Each sprite is just a UV rectangle within the
    //   atlas. SubTexture2D stores that rectangle.
    //
    // WHAT IT HOLDS:
    //
    //   - A non-owning pointer to the atlas Texture2D.
    //   - UV min/max coordinates defining the sub-region.
    //     uvMin = bottom-left corner, uvMax = top-right corner
    //     (in OpenGL's coordinate system where Y=0 is the bottom).
    //
    // HOW TO CREATE:
    //
    //   1. Raw UVs:
    //        SubTexture2D sub { atlas, {0.0, 0.5}, {0.25, 1.0} };
    //
    //   2. From a grid-based sprite sheet:
    //        auto sub = SubTexture2D::createFromGrid(
    //            atlas,
    //            { 3, 0 },       // column 3, row 0 (top-left of sheet)
    //            { 16, 32 }      // each cell is 16x32 pixels
    //        );
    //
    // LIFETIME:
    //
    //   SubTexture2D does NOT own the atlas. The Texture2D must outlive
    //   all SubTexture2D instances that reference it. SubTexture2D is
    //   a lightweight value type -- copyable, cheap to pass around.
    // -----------------------------------------------------------------

    export class SubTexture2D
    {
    public:
        // ── From explicit UV coordinates ────────────────────────
        // uvMin = bottom-left corner of the sub-region (OpenGL UV).
        // uvMax = top-right corner of the sub-region.
        SubTexture2D(
            Texture2D const& atlas,
            glm::vec2 const& uvMin,
            glm::vec2 const& uvMax
        );

        // ── From a grid-based sprite sheet ──────────────────────
        // The most common way to extract sprites. The sprite sheet
        // is divided into a uniform grid of cells.
        //
        // coords:
        //   (column, row) of the desired cell. (0,0) is the TOP-LEFT
        //   cell of the sprite sheet -- how you'd naturally read it
        //   in an image editor. Row increases downward.
        //
        // cellSize:
        //   Pixel dimensions of one grid cell (e.g., {16, 16}).
        //   Must match the actual grid spacing in the image.
        //
        // spriteSize:
        //   How many cells this sprite spans. Default {1,1} = one
        //   cell. Use {1,2} for a sprite that is 1 cell wide and
        //   2 cells tall (e.g., "big Mario" on a 16-pixel grid).
        [[nodiscard]] static SubTexture2D createFromGrid(
            Texture2D const& atlas,
            glm::vec2 const& coords,
            glm::vec2 const& cellSize,
            glm::vec2 const& spriteSize = { 1.0f, 1.0f }
        );

        // ── Accessors ───────────────────────────────────────────
        [[nodiscard]] Texture2D const& texture() const { return *m_atlas; }
        [[nodiscard]] glm::vec2 const& uvMin()   const { return m_uvMin; }
        [[nodiscard]] glm::vec2 const& uvMax()   const { return m_uvMax; }

    private:
        Texture2D const* m_atlas;       // non-owning
        glm::vec2        m_uvMin;       // bottom-left UV
        glm::vec2        m_uvMax;       // top-right UV

    }; // class SubTexture2D

} // namespace Renderer