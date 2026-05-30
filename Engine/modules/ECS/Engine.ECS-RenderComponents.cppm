module;

#include "glm/glm.hpp"

export module Engine.ECS:RenderComponents;

import Engine.Renderer;
import std;

/*═══════════════════════════════════════════════════════════════════════════════
*
*          [[nodiscard]]
*       auto Render_Ωmega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      ΩMEGAENGINE :: Render
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
    // SpriteRenderer -- tells the render system to draw this entity
    //                   as a textured or colored quad.
    //
    // The render system should query all entities that have both Transform
    // and SpriteRenderer, then calls Renderer2D::drawQuad() for each.
    //
    // If texture is nullptr, the entity draws as a solid-colored quad
    // (the batch renderer's internal 1x1 white pixel does the trick:
    // white * color = color).
    //
    // For sprite-sheet sprites, set uvMin/uvMax to the sub-region
    // extracted by SubTexture2D::createFromGrid(). The texture pointer
    // should point to the atlas Texture2D.
    //
    // LIFETIME: the Texture2D pointed to must outlive this component.
    // Typically, textures live in an asset manager or scene and persist
    // for the duration of gameplay.
    // -----------------------------------------------------------------

    export struct SpriteRenderer
    {
        glm::vec4 color { 1.0f, 1.0f, 1.0f, 1.0f };

        // Texture to sample. nullptr = color-only quad (white pixel).
        // Non-owning: the Texture2D must outlive this component (it's
        // owned by the AssetManager). This RUNTIME pointer is not
        // serializable -- texturePath below is the saveable reference.
        Renderer::Texture2D const* texture { nullptr };

        // Serializable asset id: the texture's path. On load, the scene
        // serializer resolves it back to the pointer via the AssetManager
        // (assets.load<Texture2D>(texturePath)). Empty = color-only quad.
        std::string texturePath {};

        // UV sub-region within the texture. Defaults to the full image.
        // For sprite-sheet sprites, populate these from SubTexture2D.
        glm::vec2 uvMin { 0.0f, 0.0f };
        glm::vec2 uvMax { 1.0f, 1.0f };

        float tilingFactor { 1.0f };
	}; // struct SpriteRenderer


    // -----------------------------------------------------------------
    // CameraComponent -- marks an entity as a camera viewpoint.
    //
    // The render system looks for the entity whose CameraComponent
    // has primary == true, reads its Transform for position/rotation,
    // and uses those to construct the Camera2D view-projection matrix.
    //
    // size: the orthographic half-height of the camera (world units).
    //   A size of 2.0 means the camera sees from -2 to +2 vertically.
    //
    // fixedAspectRatio: if true, the camera ignores viewport resize
    //   and maintains its original aspect ratio (letterboxing).
    //   If false (default), the camera stretches to fill the viewport.
    // -----------------------------------------------------------------

    export struct CameraComponent
    {
        float size             { 2.0f };
        bool  primary          { true };
        bool  fixedAspectRatio { false };
	}; // struct CameraComponent

} // namespace ECS