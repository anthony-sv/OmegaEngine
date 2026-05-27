module;

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

module Engine.Renderer:Camera2D;

import :Camera2D;
import std;

namespace Engine::Renderer
{

    // Omega::Constructor --------------------------------------------------------

    Camera2D::Camera2D(float aspectRatio, float size)
        : m_size        { size }
        , m_aspectRatio { aspectRatio }
    {
        recalculate();
    }

    // Omega::Mutators -----------------------------------------------------------
    // Each setter stores the new value, then rebuilds the cached VP matrix.
    // Calling recalculate() on every set is fine -- it's two 4x4 multiplies
    // (ortho + view), which is negligible compared to a single draw call.

    void Camera2D::setPosition(glm::vec2 const& position)
    {
        m_position = position;
        recalculate();
    }

    void Camera2D::setRotation(float degrees)
    {
        m_rotation = degrees;
        recalculate();
    }

    void Camera2D::setZoom(float zoom)
    {
        m_zoom = zoom;
        recalculate();
    }

    void Camera2D::setSize(float size)
    {
        m_size = size;
        recalculate();
    }

    void Camera2D::setAspectRatio(float aspectRatio)
    {
        m_aspectRatio = aspectRatio;
        recalculate();
    }

    // Omega::VP recalculation ---------------------------------------------------
    //
    // Projection (orthographic):
    //   glm::ortho maps a world-space rectangle to the [-1,+1] NDC cube.
    //   "size" is the vertical half-extent; horizontal is size * aspectRatio.
    //   Zoom divides both extents: zoom 2x halves the visible area, making
    //   everything appear twice as large.
    //
    //     halfH = size / zoom
    //     halfW = halfH * aspectRatio
    //     ortho(-halfW, halfW, -halfH, halfH, -1, 1)
    //
    // View:
    //   The camera's inverse transform, built explicitly instead of calling
    //   glm::inverse() (which is overkill for a 2D translate + rotate).
    //
    //     V = R(-theta) * T(-position)
    //
    //   Read right-to-left: first slide the world so the camera is at the
    //   origin (translate by -position), then spin it so the camera's "up"
    //   points along +Y (rotate by -theta around Z).
    //
    // ViewProjection:
    //   VP = P * V. The vertex shader does:
    //     gl_Position = u_ViewProjection * vec4(worldPos, 0.0, 1.0);

    void Camera2D::recalculate()
    {
        float const halfH = m_size / m_zoom;
        float const halfW = halfH * m_aspectRatio;

        glm::mat4 const projection = glm::ortho(
            -halfW, halfW,      // left, right
            -halfH, halfH,      // bottom, top
            -1.0f,  1.0f        // near, far (2D: trivial depth range)
        );

        // View = R(-theta) * T(-pos)
        // glm::rotate/translate post-multiply, so building on identity:
        //   view = I
        //   view = rotate(view, ...)    --> view = R
        //   view = translate(view, ...) --> view = R * T
        glm::mat4 view { 1.0f };
        view = glm::rotate(
            view,
            glm::radians(-m_rotation),
            glm::vec3 { 0.0f, 0.0f, 1.0f }
        );
        view = glm::translate(
            view,
            glm::vec3 { -m_position.x, -m_position.y, 0.0f }
        );

        m_viewProjection = projection * view;
    }

} // namespace Renderer