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

    // Per-scene setup. Systems + input bindings are engine CODE; the
    // ENTITIES come purely from the scene file (scenes/<name>.json) --
    // no code-built fallback. Textures referenced by the scene resolve
    // through the AssetManager during load.
    auto setup = [this](Engine::Scene::World& w, std::string nextScene)
    {
        std::println("[Ω::Sandbox] enter scene '{}'", w.name());

        w.addSystem<Engine::Systems::MovementSystem>();
        w.addSystem<Engine::Systems::AnimationSystem>();

        w.actions().bind(Engine::Core::Key::Space, "NextScene");
        w.setOnAction([this, nextScene](Engine::Scene::World&, Engine::Core::ActionEvent const& a)
        {
            if (a.name == "NextScene" && a.started)
                m_sceneManager.switchTo(nextScene);
        });

        auto& assets    = Engine::Core::Application::get().assets();
        auto const file = "scenes/" + w.name() + ".json";
        if (auto r = Engine::Scene::SceneSerializer::load(w, file, assets); !r)
            std::println(std::cerr, "[Ω::Sandbox] could not load '{}': {}", file, r.error().message);
    };

    auto onExit = [](Engine::Scene::World& w)
    {
        std::println("[Ω::Sandbox] exit  scene '{}'", w.name());
        w.clear();
    };

    {
        auto& grid = m_sceneManager.create("Grid");
        grid.setOnEnter([setup](Engine::Scene::World& w) { setup(w, "Ring"); });
        grid.setOnExit(onExit);
    }
    {
        auto& ring = m_sceneManager.create("Ring");
        ring.setOnEnter([setup](Engine::Scene::World& w) { setup(w, "Grid"); });
        ring.setOnExit(onExit);
    }

    m_sceneManager.switchTo("Grid");
    std::println("[Ω::SandboxLayer] scenes registered — press SPACE to switch, Ctrl+S to save");
}

void SandboxLayer::onDetach()
{
    m_camera.reset();
    Engine::Renderer::Renderer2D::shutdown();
    std::println("[Ω::SandboxLayer] detached");
}

void SandboxLayer::onUpdate(float dt)
{
    using Input = Engine::Core::Input;
    using Engine::Core::Key;

    // Ctrl+S saves the active world back to its scene file.
    if (Input::isKeyDown(Key::LeftControl) && Input::wasKeyPressed(Key::S))
    {
        if (auto* w = m_sceneManager.active())
        {
            auto const file = "scenes/" + w->name() + ".json";
            if (auto r = Engine::Scene::SceneSerializer::save(*w, file); !r)
                std::println(std::cerr, "[Ω::SandboxLayer] save failed: {}", r.error().message);
        }
    }

    // Space switches scenes (bound per scene to "NextScene"). The manager
    // applies any pending switch at the frame boundary, then ticks systems.
    m_sceneManager.onUpdate(dt);
}

void SandboxLayer::onRender(float /*alpha*/)
{
    auto* scene = m_sceneManager.active();
    if (!scene || !m_camera)
        return;

    Engine::Systems::RenderSystem::render(scene->registry(), *m_camera);
}