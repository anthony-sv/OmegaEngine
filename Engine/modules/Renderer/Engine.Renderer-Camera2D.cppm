module;

#include "glm/glm.hpp"

export module Engine.Renderer:Camera2D;

import std;

/*===============================================================================
*
*          [[nodiscard]]
*       auto Render_Omega() {
*    glm::mat4            vk::Image
*   ID3D12Device        MTL::Device
*  float3                  uint4
*  half                      short      OMEGAENGINE :: Camera2D
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
    // Camera2D -- orthographic camera for 2D world-to-clip transform.
    //
    // Provides a combined View-Projection (VP) matrix:
    //
    //   Projection (ortho)
    //     Maps a rectangle of world space into the [-1,+1] NDC cube.
    //     "size" is the vertical half-extent in world units (matches
    //     Unity's orthographicSize convention). Horizontal extent is
    //     derived from the aspect ratio. Zoom divides the extents --
    //     zoom 2x means you see half the world (things look 2x bigger).
    //
    //   View
    //     The inverse of the camera's own transform: translates by
    //     -position, rotates by -rotation. Cheaper than glm::inverse()
    //     and exact for 2D (no floating-point drift).
    //
    //   VP = projection * view
    //     The vertex shader multiplies:
    //       gl_Position = u_ViewProjection * vec4(worldPos, 0.0, 1.0);
    //
    // No GL resources are held -- Camera2D is a pure math type (rule
    // of zero). Copy/move are compiler-generated and trivially correct.
    // -----------------------------------------------------------------

    export class Camera2D
    {
    public:
        // aspectRatio = viewport width / height.
        // size = vertical half-extent in world units (default 1.0 so
        // that world coords [-1,+1] map to NDC [-1,+1] at zoom 1x).
        explicit Camera2D(float aspectRatio, float size = 1.0f);

        // -- Mutators (each triggers a VP recalculation) -------------
        void setPosition(glm::vec2 const& position);
        void setRotation(float degrees);
        void setZoom(float zoom);
        void setSize(float size);
        void setAspectRatio(float aspectRatio);

        // -- Accessors -----------------------------------------------
        [[nodiscard]] glm::vec2 const& position()  const { return m_position; }
        [[nodiscard]] float rotation()             const { return m_rotation; }
        [[nodiscard]] float zoom()                 const { return m_zoom; }
        [[nodiscard]] float size()                 const { return m_size; }
        [[nodiscard]] float aspectRatio()          const { return m_aspectRatio; }

        // The combined View-Projection matrix. Upload to the shader
        // as u_ViewProjection once per frame before drawing.
        [[nodiscard]] glm::mat4 const& viewProjection() const { return m_viewProjection; }

        // The View and Projection matrices SEPARATELY. The shader only ever
        // needs the combined VP, but tools that expect them split -- notably
        // ImGuizmo -- read these. (Cached alongside VP in recalculate().)
        [[nodiscard]] glm::mat4 const& view()       const { return m_view; }
        [[nodiscard]] glm::mat4 const& projection() const { return m_projection; }

    private:
        // Recomputes the cached matrices from the current parameters.
        void recalculate();

        glm::vec2 m_position    { 0.0f, 0.0f };
        float m_rotation        { 0.0f };           // degrees, CCW positive
        float m_zoom            { 1.0f };           // >1 = closer, <1 = further
        float m_size            { 1.0f };           // vertical half-extent (world units)
        float m_aspectRatio     { 16.0f / 9.0f };

        glm::mat4 m_view           { 1.0f };
        glm::mat4 m_projection     { 1.0f };
        glm::mat4 m_viewProjection { 1.0f };

    }; // class Camera2D

} // namespace Renderer