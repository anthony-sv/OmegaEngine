module;

#include "glm/glm.hpp"

module SandboxLayer;

import Engine.Core;
import Engine.Renderer;
import Engine.ECS;
import Engine.Scene;
import Engine.Systems;
import std;

namespace
{
    // Window is created at this size (see SandboxApp). The camera's
    // aspect ratio is derived from it.
    constexpr float WindowWidth  = 1280.0f;
    constexpr float WindowHeight = 720.0f;

    // ── Scene builders ──────────────────────────────────────────────
    // A scene is just data: which entities and which systems. These
    // free functions populate a Scene -- no Scene subclassing. They run
    // from each scene's onEnter hook, so the content is (re)built every
    // time the scene becomes active and torn down (Scene::clear) on exit.

    // "Grid": a 6x4 color gradient + a spinning white quad in the middle.
    void buildGridScene(Engine::Scene::World& scene)
    {
        constexpr int   cols    = 6;
        constexpr int   rows    = 4;
        constexpr float spacing = 1.1f;

        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                float const x = (static_cast<float>(c) - (cols - 1) * 0.5f) * spacing;
                float const y = (static_cast<float>(r) - (rows - 1) * 0.5f) * spacing;

                auto tile = scene.createEntity(std::format("Tile [{},{}]", c, r));
                tile.add<Engine::ECS::Transform>(Engine::ECS::Transform{
                    .position = { x, y }, .rotation = 0.0f, .scale = { 0.8f, 0.8f }
                });
                tile.add<Engine::ECS::SpriteRenderer>(Engine::ECS::SpriteRenderer{
                    .color = {
                        static_cast<float>(c) / (cols - 1),
                        static_cast<float>(r) / (rows - 1),
                        0.6f, 1.0f
                    }
                });
            }
        }

        auto spinner = scene.createEntity("Spinner");
        spinner.add<Engine::ECS::Transform>(Engine::ECS::Transform{
            .position = { 0.0f, 0.0f }, .rotation = 0.0f, .scale = { 1.0f, 1.0f }
        });
        spinner.add<Engine::ECS::SpriteRenderer>(Engine::ECS::SpriteRenderer{
            .color = { 1.0f, 1.0f, 1.0f, 1.0f }
        });
        spinner.add<Engine::ECS::Velocity2D>(Engine::ECS::Velocity2D{
            .linear = { 0.0f, 0.0f }, .angular = 90.0f
        });

        scene.addSystem<Engine::Systems::MovementSystem>();
    }

    // "Ring": 8 quads arranged in a circle, each spinning, rainbow hued.
    void buildRingScene(Engine::Scene::World& scene)
    {
        constexpr int   count  = 8;
        constexpr float radius = 1.6f;
        constexpr float tau    = 6.2831853f;

        for (int i = 0; i < count; ++i)
        {
            float const t = static_cast<float>(i) / count;
            float const a = t * tau;

            auto node = scene.createEntity(std::format("Node {}", i));
            node.add<Engine::ECS::Transform>(Engine::ECS::Transform{
                .position = { radius * std::cos(a), radius * std::sin(a) },
                .rotation = 0.0f,
                .scale    = { 0.5f, 0.5f }
            });
            // Hue around the wheel: cheap RGB from the angle.
            node.add<Engine::ECS::SpriteRenderer>(Engine::ECS::SpriteRenderer{
                .color = {
                    0.5f + 0.5f * std::cos(a),
                    0.5f + 0.5f * std::cos(a + tau / 3.0f),
                    0.5f + 0.5f * std::cos(a + 2.0f * tau / 3.0f),
                    1.0f
                }
            });
            node.add<Engine::ECS::Velocity2D>(Engine::ECS::Velocity2D{
                .linear = { 0.0f, 0.0f }, .angular = (i % 2 == 0 ? 120.0f : -120.0f)
            });
        }

        scene.addSystem<Engine::Systems::MovementSystem>();
    }
}

SandboxLayer::SandboxLayer()
    : ILayer { "Ω::SandboxLayer" }
{}

void SandboxLayer::onAttach()
{
    std::println("[Ω::SandboxLayer] attached");

    auto initResult = Engine::Renderer::Renderer2D::init();
    if (!initResult)
    {
        std::println(std::cerr, "[Ω::SandboxLayer] Renderer2D init failed: {}",
                     initResult.error().message);
        return;
    }

    m_camera.emplace(WindowWidth / WindowHeight, 3.0f);

    // ── Register the scenes ─────────────────────────────────────
    // Each scene gets its content via an onEnter hook (build) and is
    // wiped via an onExit hook (teardown). A single shared onExit works
    // for both -- it just clears whatever scene is leaving.
    auto onExit = [](Engine::Scene::World& scene)
    {
        std::println("[Ω::Sandbox] exit  scene '{}' (clearing)", scene.name());
        scene.clear();
    };

    {
        auto& grid = m_sceneManager.create("Grid");
        grid.setOnEnter([](Engine::Scene::World& s)
        {
            std::println("[Ω::Sandbox] enter scene '{}' (building grid)", s.name());
            buildGridScene(s);
        });
        grid.setOnExit(onExit);
    }
    {
        auto& ring = m_sceneManager.create("Ring");
        ring.setOnEnter([](Engine::Scene::World& s)
        {
            std::println("[Ω::Sandbox] enter scene '{}' (building ring)", s.name());
            buildRingScene(s);
        });
        ring.setOnExit(onExit);
    }

    // Pick the starting scene (applied on the first onUpdate).
    m_sceneManager.switchTo("Grid");

    std::println("[Ω::SandboxLayer] {} scenes registered — starting on 'Grid'", m_sceneManager.sceneCount());
}

void SandboxLayer::onDetach()
{
    m_camera.reset();
    Engine::Renderer::Renderer2D::shutdown();
    std::println("[Ω::SandboxLayer] detached");
}

void SandboxLayer::onUpdate(float dt)
{
    // Demo driver: every 3 seconds, request the other scene. switchTo()
    // is deferred -- the manager applies it at the top of onUpdate, so
    // the actual exit()/enter() happen at a safe frame boundary.
    m_switchTimer += dt;
    if (m_switchTimer >= 3.0f)
    {
        m_switchTimer = 0.0f;
        m_showGrid    = !m_showGrid;
        m_sceneManager.switchTo(m_showGrid ? "Grid" : "Ring");
    }

    // Applies any pending switch, then ticks the active scene's systems
    // (MovementSystem spins the quads).
    m_sceneManager.onUpdate(dt);
}

void SandboxLayer::onRender(float /*alpha*/)
{
    auto* scene = m_sceneManager.active();
    if (!scene || !m_camera)
        return;

    // Draw whatever scene is currently active. The layer doesn't care
    // which one -- it just renders the active registry.
    Engine::Systems::RenderSystem::render(scene->registry(), *m_camera);
}