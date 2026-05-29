module;

#include "glm/glm.hpp"

module SandboxLayer;

import Engine.Core;
import Engine.Renderer;
import Engine.ECS;
import Engine.Systems;
import std;

namespace
{
    // Window is created at this size (see SandboxApp). The camera's
    // aspect ratio is derived from it. (Resizing the window won't
    // re-fit the scene yet -- that needs a framebuffer-resize event,
    // which the bare Sandbox doesn't wire up.)
    constexpr float WindowWidth  = 1280.0f;
    constexpr float WindowHeight = 720.0f;
}

SandboxLayer::SandboxLayer()
    : ILayer { "Ω::SandboxLayer" }
{}

void SandboxLayer::onAttach()
{
    std::println("[Ω::SandboxLayer] attached");

    // ── Batch renderer ──────────────────────────────────────────
    // Same one-time GPU resource setup the Editor does. We're a plain
    // consumer here -- this is the engine's public init path.
    auto initResult = Engine::Renderer::Renderer2D::init();
    if (!initResult)
    {
        std::println(std::cerr, "[Ω::SandboxLayer] Renderer2D init failed: {}",
                     initResult.error().message);
        return;
    }

    // ── Camera ──────────────────────────────────────────────────
    // size = 3 → the view spans [-3, +3] vertically in world units.
    m_camera.emplace(WindowWidth / WindowHeight, 3.0f);

    // ── Build the ECS scene ─────────────────────────────────────
    // A grid of colored quads. Each cell is an entity with a
    // Transform (where/how big) and a SpriteRenderer (what color).
    // Color is derived from the grid position so we get a gradient.
    constexpr int   cols    = 6;
    constexpr int   rows    = 4;
    constexpr float spacing = 1.1f;

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            float const x = (static_cast<float>(c) - (cols - 1) * 0.5f) * spacing;
            float const y = (static_cast<float>(r) - (rows - 1) * 0.5f) * spacing;

            auto tile = m_registry.create(std::format("Tile [{},{}]", c, r));

            tile.add<Engine::ECS::Transform>(Engine::ECS::Transform{
                .position = { x, y },
                .rotation = 0.0f,
                .scale    = { 0.8f, 0.8f }
            });

            // Gradient: red across columns, green up rows, fixed blue.
            tile.add<Engine::ECS::SpriteRenderer>(Engine::ECS::SpriteRenderer{
                .color = {
                    static_cast<float>(c) / (cols - 1),
                    static_cast<float>(r) / (rows - 1),
                    0.6f,
                    1.0f
                }
            });
        }
    }

    // ── The spinner ─────────────────────────────────────────────
    // One white quad in the middle that we rotate and pulse every
    // frame in onUpdate -- proof that mutating components through an
    // Entity handle drives the render output live.
    m_spinner = m_registry.create("Spinner");
    m_spinner.add<Engine::ECS::Transform>(Engine::ECS::Transform{
        .position = { 0.0f, 0.0f },
        .rotation = 0.0f,
        .scale    = { 1.0f, 1.0f }
    });
    m_spinner.add<Engine::ECS::SpriteRenderer>(Engine::ECS::SpriteRenderer{
        .color = { 1.0f, 1.0f, 1.0f, 1.0f }
    });

    std::println("[Ω::SandboxLayer] scene built — {} entities",
                 m_registry.entityCount());
}

void SandboxLayer::onDetach()
{
    m_camera.reset();

    // Release the batch renderer's GPU resources before the context
    // goes away. The Registry (and all its entities) tears itself down
    // when this layer is destroyed -- rule of zero, nothing to do here.
    Engine::Renderer::Renderer2D::shutdown();

    std::println("[Ω::SandboxLayer] detached");
}

void SandboxLayer::onUpdate(float dt)
{
    m_elapsed += dt;

    // Animate the spinner by writing to its components through the
    // handle. valid() guards against the entity not existing (e.g.
    // if init bailed out before creating it).
    if (m_spinner.valid())
    {
        auto& transform = m_spinner.get<Engine::ECS::Transform>();

        transform.rotation = m_elapsed * 90.0f;                   // 90°/sec

        float const pulse  = 1.0f + 0.3f * std::sin(m_elapsed * 3.0f);
        transform.scale    = { pulse, pulse };
    }
}

void SandboxLayer::onRender(float /*alpha*/)
{
    if (!m_camera)
        return;

    // The app loop already cleared the window's default framebuffer,
    // so we just hand the scene to the RenderSystem. It opens its own
    // batch, iterates view<Transform, SpriteRenderer>, draws every
    // entity, and flushes -- straight to the screen, no FBO.
    Engine::Systems::RenderSystem::render(m_registry, *m_camera);
}