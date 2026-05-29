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

    // Sprite sheet for the animated demo entity (optional -- the scenes
    // still work without it). 128x64, two 64x64 character cells.
    if (auto sheet = Engine::Renderer::Texture2D::create("assets/textures/Sheet.png"))
        m_sheet.emplace(std::move(*sheet));
    else
        std::println(std::cerr, "[Ω::SandboxLayer] sheet load failed: {}", sheet.error().message);

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
        grid.setOnEnter([this](Engine::Scene::World& w)
        {
            std::println("[Ω::Sandbox] enter scene '{}' (building grid)", w.name());
            buildGridScene(w);

            // ── Animated sprite ──────────────────────
            // One entity that cycles between the sheet's two 64x64 cells.
            // The AnimationSystem advances SpriteAnimation -> writes the
            // SpriteRenderer's UVs; the RenderSystem then draws it.
            if (m_sheet)
            {
                namespace R = Engine::Renderer;
                auto const f0 = R::SubTexture2D::createFromGrid(*m_sheet, { 0.0f, 0.0f }, { 64.0f, 64.0f });
                auto const f1 = R::SubTexture2D::createFromGrid(*m_sheet, { 1.0f, 0.0f }, { 64.0f, 64.0f });

                auto hero = w.createEntity("Animated Hero");
                hero.add<Engine::ECS::Transform>(Engine::ECS::Transform{
                    .position = { 0.0f, 2.0f }, .rotation = 0.0f, .scale = { 1.0f, 1.0f } });
                hero.add<Engine::ECS::SpriteRenderer>(Engine::ECS::SpriteRenderer{
                    .texture = &*m_sheet, .uvMin = f0.uvMin(), .uvMax = f0.uvMax() });
                hero.add<Engine::ECS::SpriteAnimation>(Engine::ECS::SpriteAnimation{
                    .frames = {
                        { .uvMin = f0.uvMin(), .uvMax = f0.uvMax() },
                        { .uvMin = f1.uvMin(), .uvMax = f1.uvMax() },
                    },
                    .frameDuration = 0.5f,
                    .looping       = true,
                    .playing       = true });

                w.addSystem<Engine::Systems::AnimationSystem>();
            }

            // Per-scene binding: Space -> "NextScene". The handler fires
            // (via the EventBus) when the action triggers while this scene
            // is active, and switches to the other scene.
            w.actions().bind(Engine::Core::Key::Space, "NextScene");
            w.setOnAction([this](Engine::Scene::World&, Engine::Core::ActionEvent const& a)
            {
                if (a.name == "NextScene" && a.started)
                    m_sceneManager.switchTo("Ring");
            });
        });
        grid.setOnExit(onExit);
    }
    {
        auto& ring = m_sceneManager.create("Ring");
        ring.setOnEnter([this](Engine::Scene::World& w)
        {
            std::println("[Ω::Sandbox] enter scene '{}' (building ring)", w.name());
            buildRingScene(w);

            w.actions().bind(Engine::Core::Key::Space, "NextScene");
            w.setOnAction([this](Engine::Scene::World&, Engine::Core::ActionEvent const& a)
            {
                if (a.name == "NextScene" && a.started)
                    m_sceneManager.switchTo("Grid");
            });
        });
        ring.setOnExit(onExit);
    }

    // Pick the starting scene (applied on the first onUpdate).
    m_sceneManager.switchTo("Grid");

    std::println("[Ω::SandboxLayer] {} scenes registered — press SPACE to switch (starting on 'Grid')", m_sceneManager.sceneCount());
}

void SandboxLayer::onDetach()
{
    m_camera.reset();
    Engine::Renderer::Renderer2D::shutdown();
    std::println("[Ω::SandboxLayer] detached");
}

void SandboxLayer::onUpdate(float dt)
{
    // Scene switching is now driven by INPUT: pressing Space fires the
    // "NextScene" action (bound per scene in onAttach), whose handler
    // calls switchTo(). Here we just tick the manager -- it applies any
    // pending switch at the frame boundary, then runs the active scene's
    // systems (MovementSystem spins the quads).
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