module;

#include "glm/glm.hpp"

export module Engine.ECS:TilemapComponents;

import Engine.Renderer;   // Renderer::Texture2D (atlas / collection tiles)
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Tilemap
*   half                    short
*    vec2                  ivec2
*     u32                  i32
*  _________;          ________;
* [RayTracing]       [Rasterizer]
*
════════════════════════════════════════════════════════════════════════════════*/

namespace Engine::ECS
{

    // -----------------------------------------------------------------
    // TilemapComponent -- a grid of tiles drawn from a tileset.
    //
    // A tilemap is the cheap, data-driven way to build a level: instead
    // of one entity per visible block, ONE entity holds a 2D grid of
    // integer tile IDs. The render system walks the grid and draws one
    // quad per non-empty cell -- all in the existing batch.
    //
    // TWO SOURCE MODES (a map picks ONE -- they are not mixed):
    //
    //   Atlas      -- the OPTIMIZED path: every tile is a uniform CELL of a
    //                 single texture, sliced on a tilePixelSize grid. One
    //                 texture bind -> the whole map is ONE draw call. Every
    //                 tile is 1x1 cell. (A lone 64x64 image is a 1-cell
    //                 atlas.) Best for big repetitive terrain.
    //   Collection -- the GENERAL path: a list of tile DEFINITIONS, each a
    //                 {texture, uv-region, footprint}. A def can come from a
    //                 loose image OR a sub-region of a sheet, and can SPAN
    //                 several cells (footprint, e.g. 2x1 for a 128x64 cloud).
    //                 One map freely mixes multi-cell tiles, single tiles,
    //                 and empties. Costs up to one bind per distinct texture
    //                 (the batcher flushes at 32).
    //
    // THE GRID (shared by both modes):
    //   `tiles` is ROW-MAJOR, size dimensions.x * dimensions.y, indexed
    //   tiles[y*w + x]. Cell (0,0) is the BOTTOM-LEFT (x right, y up). A
    //   value of -1 = EMPTY; >= 0 = a tile id (an Atlas cell index, or an
    //   index into `tileDefs` for Collection). A multi-cell tile is stored
    //   only at its ANCHOR cell (bottom-left); it draws spanning its
    //   footprint from there. For 2D access use tilesView() (std::mdspan).
    //
    // SOLID TILES (collision):
    //   `solidTiles` lists tile IDs treated as solid -- the physics system
    //   builds static colliders for every cell holding one.
    // -----------------------------------------------------------------

    export struct TilemapComponent
    {
        enum class Source : std::uint8_t
        {
            Atlas,        // one sliced texture, uniform 1x1 cells (fast, 1 draw call)
            Collection    // a list of tile DEFINITIONS (flexible, multi-cell)
        };

        // One Collection tile: where its pixels come from + how many grid
        // cells it occupies. uv is the region within `texture` (full [0,1]
        // for a loose image; a sub-rect for an atlas-sourced tile).
        // footprint is the size in CELLS (e.g. {2,1} = two cells wide).
        struct TileDef
        {
            std::string                texturePath {};             // serializable source
            Renderer::Texture2D const* texture { nullptr };        // runtime
            glm::vec2                  uvMin { 0.0f, 0.0f };       // region in the texture
            glm::vec2                  uvMax { 1.0f, 1.0f };
            glm::ivec2                 footprint { 1, 1 };         // size in CELLS
        };

        Source source { Source::Atlas };

        // -- Atlas mode (optimized) ---------------------------------------
        std::string                atlasPath {};              // serializable id
        Renderer::Texture2D const* atlas { nullptr };         // runtime, not serialized
        glm::ivec2                 tilePixelSize { 64, 64 };  // cell size, PIXELS

        // -- Collection mode (general) ------------------------------------
        // tileDefs[id] defines tile id (texture + uv + footprint).
        std::vector<TileDef> tileDefs {};

        // -- Shared -------------------------------------------------------
        float            tileWorldSize { 1.0f };   // size of one tile, WORLD units
        glm::ivec2       dimensions { 20, 15 };    // map size in TILES (w, h)
        std::vector<int> tiles {};                 // row-major; -1 empty, >=0 tile id
        std::vector<int> solidTiles {};            // tile ids treated as solid

        // -- Helpers ------------------------------------------------------

        // Cells the grid should hold for its current dimensions.
        [[nodiscard]] std::size_t cellCount() const
        {
            return static_cast<std::size_t>(dimensions.x) * static_cast<std::size_t>(dimensions.y);
        }

        // True if (x, y) is inside the grid bounds.
        [[nodiscard]] bool inBounds(int x, int y) const
        {
            return x >= 0 && y >= 0 && x < dimensions.x && y < dimensions.y;
        }

        // Tile id at (x, y), or -1 if out of bounds / unset. Safe even if
        // the backing store hasn't been sized to cellCount() yet.
        [[nodiscard]] int at(int x, int y) const
        {
            if (!inBounds(x, y))
                return -1;
            auto const i = static_cast<std::size_t>(y) * dimensions.x + x;
            return i < tiles.size() ? tiles[i] : -1;
        }

        // Set the tile at (x, y) (no-op if out of bounds). Sizes the backing
        // store to cellCount() (filled with -1) on first write, so a fresh
        // tilemap doesn't need its vector pre-filled.
        void set(int x, int y, int id)
        {
            if (!inBounds(x, y))
                return;
            if (tiles.size() != cellCount())
                tiles.resize(cellCount(), -1);
            tiles[static_cast<std::size_t>(y) * dimensions.x + x] = id;
        }

        // A C++23 std::mdspan VIEW over the flat grid for 2D [y, x] access
        // (g[y, x]). Non-owning -- `tiles` still owns the storage. Only valid
        // once the store is sized to cellCount() (e.g. after any set() or the
        // serializer's resize); callers in doubt should use at()/set().
        [[nodiscard]] auto tilesView()
        {
            return std::mdspan<int, std::dextents<std::size_t, 2>> {
                tiles.data(),
                static_cast<std::size_t>(dimensions.y),
                static_cast<std::size_t>(dimensions.x)
            };
        }
        [[nodiscard]] auto tilesView() const
        {
            return std::mdspan<int const, std::dextents<std::size_t, 2>> {
                tiles.data(),
                static_cast<std::size_t>(dimensions.y),
                static_cast<std::size_t>(dimensions.x)
            };
        }

        // Is tile `id` flagged solid?
        [[nodiscard]] bool isSolid(int id) const
        {
            return id >= 0
                && std::ranges::find(solidTiles, id) != solidTiles.end();
        }

        // How many cells tile `id` occupies. Collection tiles carry a
        // footprint; Atlas tiles are always one cell.
        [[nodiscard]] glm::ivec2 footprintOf(int id) const
        {
            if (source == Source::Collection
                && id >= 0 && static_cast<std::size_t>(id) < tileDefs.size())
                return tileDefs[id].footprint;
            return { 1, 1 };
        }
    }; // struct TilemapComponent

} // namespace ECS