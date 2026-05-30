module SandboxLayer;

import Engine.Core;
import Engine.Renderer;
import Engine.ECS;
import Engine.Scene;
import Engine.Systems;
import std;

SandboxLayer::SandboxLayer(Engine::Scene::Project project)
    : ILayer    { "Ω::SandboxLayer" }
    , m_project { std::move(project) }
{}

void SandboxLayer::onAttach()
{
    std::println("[Ω::SandboxLayer] attached — playing project '{}'", m_project.name());

    auto initResult = Engine::Renderer::Renderer2D::init();
    if (!initResult)
    {
        std::println(std::cerr, "[Ω::SandboxLayer] Renderer2D init failed: {}",
                     initResult.error().message);
        return;
    }

    // Aspect ratio comes from the project's window config.
    auto const& win = m_project.window();
    m_camera.emplace(static_cast<float>(win.width) / static_cast<float>(win.height), 3.0f);

    // A copy of the project's scene list, captured by the per-scene
    // hooks so SPACE can cycle through it.
    auto const sceneNames = m_project.scenes();

    // Per-scene setup. Systems + input bindings are engine CODE; the
    // ENTITIES come purely from the scene file (scenes/<name>.json) --
    // no code-built fallback. Textures referenced by the scene resolve
    // through the AssetManager during load. (Working dir == project
    // root, so the relative path lands inside the project.)
    auto setup = [this, sceneNames](Engine::Scene::World& w)
    {
        std::println("[Ω::Sandbox] enter scene '{}'", w.name());

        w.addSystem<Engine::Systems::MovementSystem>();
        w.addSystem<Engine::Systems::AnimationSystem>();

        // SPACE -> cycle to the NEXT scene in the project's list. The
        // "next" is computed from data (this world's position in the
        // list), so no scene name is hardcoded.
        w.actions().bind(Engine::Core::Key::Space, "NextScene");
        w.setOnAction([this, sceneNames](Engine::Scene::World& world, Engine::Core::ActionEvent const& a)
        {
            if (a.name != "NextScene" || !a.started || sceneNames.size() < 2)
                return;

            auto const it  = std::ranges::find(sceneNames, world.name());
            auto const idx = (it == sceneNames.end())
                           ? std::size_t { 0 }
                           : static_cast<std::size_t>(std::distance(sceneNames.begin(), it));
            m_sceneManager.switchTo(sceneNames[(idx + 1) % sceneNames.size()]);
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

    // One world per scene the project declares.
    for (auto const& name : sceneNames)
    {
        auto& world = m_sceneManager.create(name);
        world.setOnEnter(setup);
        world.setOnExit(onExit);
    }

    if (!sceneNames.empty())
        m_sceneManager.switchTo(m_project.startupScene());

    std::println("[Ω::SandboxLayer] {} scene(s) registered — SPACE to cycle, Ctrl+S to save",
                 sceneNames.size());
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