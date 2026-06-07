module SandboxLayer;

import Engine.Core;
import Engine.Renderer;
import Engine.ECS;
import Engine.Scene;
import Engine.Systems;
import Engine.Physics;
import Engine.Scripting;
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

    // Log physics collisions/triggers (proves the EventBus pipeline). The
    // subscription lives for the layer's lifetime.
    auto& bus = Engine::Core::Application::get().eventBus();
    auto nameOf = [](Engine::ECS::Entity e) -> std::string
    {
        return (e.valid() && e.has<Engine::ECS::NameComponent>())
             ? e.get<Engine::ECS::NameComponent>().name : std::string { "?" };
    };
    bus.subscribe<Engine::Physics::CollisionEnterEvent>(
        [nameOf](Engine::Physics::CollisionEnterEvent const& e)
        {
            std::println("[Ω::Physics] collision {} <-> {}  (normal {:.2f}, {:.2f})",
                         nameOf(e.a), nameOf(e.b), e.manifold.normal.x, e.manifold.normal.y);
        });
    bus.subscribe<Engine::Physics::TriggerEnterEvent>(
        [nameOf](Engine::Physics::TriggerEnterEvent const& e)
        {
            std::println("[Ω::Physics] trigger {} entered by {}", nameOf(e.sensor), nameOf(e.other));
        });

    // The project's scene list -- one world is created per entry below.
    auto const sceneNames = m_project.scenes();

    // Per-scene setup. Systems + input bindings are engine CODE; the
    // ENTITIES come purely from the scene file (scenes/<name>.json) --
    // no code-built fallback. Textures referenced by the scene resolve
    // through the AssetManager during load. (Working dir == project
    // root, so the relative path lands inside the project.)
    auto setup = [this](Engine::Scene::World& w)
    {
        std::println("[Ω::Sandbox] enter scene '{}'", w.name());

        // FIRST: snapshot transforms for render interpolation (must run
        // before any system that moves an entity).
        w.addSystem<Engine::Systems::InterpolationSystem>();

        w.addSystem<Engine::Systems::MovementSystem>();
        w.addSystem<Engine::Systems::AnimationSystem>();

        // C# scripts (gameplay logic lives in the project's managed assembly).
        // managedDir = exe dir (OmegaEngine.dll + BuildScripts.cs, deployed
        // post-build); scriptsDir = the project's script SOURCE folder (cwd =
        // project root). The engine compiles it and hot-reloads on edits.
        auto& scripts = w.addSystem<Engine::Scripting::ScriptSystem>(
            Engine::Core::Paths::executableDir(),
            "scripts");

        // Physics is universal engine code; entities opt IN by carrying a
        // RigidBody2D + collider. Scenes without physics bodies (Grid/Ring)
        // just step an empty world -- effectively free.
        auto& bus     = Engine::Core::Application::get().eventBus();
        auto& physics = w.addSystem<Engine::Physics::PhysicsSystem>(bus);

        // Give scripts the body API (velocity/impulse/force) + collision and
        // trigger callbacks. Done after BOTH systems exist.
        scripts.usePhysics(physics.world(), bus);

        // NOTE: the runtime does NOT bind any scene-switching keys. Changing
        // scenes is GAMEPLAY -- a project script drives it (or the editor) --
        // so the runtime never steals a key from the project.

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

    std::println("[Ω::SandboxLayer] {} scene(s) registered — Ctrl+S to save",
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

    // A follow-cam script (if any) steers this camera; bind it before ticking.
    if (m_camera)
        Engine::Scripting::ScriptHost::instance().bindCamera(&*m_camera);

    // Space switches scenes (bound per scene to "NextScene"). The manager
    // applies any pending switch at the frame boundary, then ticks systems.
    m_sceneManager.onUpdate(dt);
}

void SandboxLayer::onRender(float alpha)
{
    auto* scene = m_sceneManager.active();
    if (!scene || !m_camera)
        return;

    // Tilemaps are the background layer -- drawn first, sprites on top.
    Engine::Systems::RenderSystem::renderTilemaps(scene->registry(), *m_camera);

    // alpha (the fixed-step sub-frame fraction) drives render interpolation.
    Engine::Systems::RenderSystem::render(scene->registry(), *m_camera, alpha);

    // F2 -> screenshot the window (default framebuffer, captured before swap).
    if (Engine::Core::Input::wasKeyPressed(Engine::Core::Key::F2))
    {
        auto& win = Engine::Core::Application::get().window();
        auto const path = Engine::Renderer::Screenshot::timestamped();
        if (auto r = Engine::Renderer::Screenshot::capture(path, 0, 0, win.width(), win.height()); !r)
            std::println(std::cerr, "[Ω::Sandbox] screenshot failed: {}", r.error().message);
    }
}