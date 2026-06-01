module;

#include "imgui.h"
#include "glm/glm.hpp"
#include "IconsFontAwesome6.h"
#ifdef _WIN32
#   include "TitlebarState.hpp"
#endif

module EditorLayer;

import Engine.Core;
import Engine.Renderer;
import Engine.ECS;
import Engine.Scene;
import Engine.Systems;

namespace
{
    // Draw editable ImGui fields for each component the entity has.
    // There is no C++ reflection, so this is the standard hand-written
    // dispatch: check has<T>(), then draw fields bound to get<T>().
    // get<T>() returns a reference, so the widgets edit the component
    // in place -- changes are live in the viewport next frame.
    void drawInspector(Engine::ECS::Entity entity)
    {
        using namespace Engine::ECS;

        if (entity.has<NameComponent>())
        {
            auto& name = entity.get<NameComponent>();
            char buf[128] = {};
            name.name.copy(buf, sizeof(buf) - 1);
            if (ImGui::InputText("Name", buf, sizeof(buf)))
                name.name = buf;
            ImGui::Separator();
        }

        if (entity.has<Transform>())
        {
            auto& t = entity.get<Transform>();
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat2("Position", &t.position.x, 0.05f);
                ImGui::DragFloat ("Rotation", &t.rotation,   1.0f);
                ImGui::DragFloat2("Scale",    &t.scale.x,    0.05f, 0.01f, 100.0f);
            }
        }

        if (entity.has<SpriteRenderer>())
        {
            auto& s = entity.get<SpriteRenderer>();
            if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ColorEdit4("Color",  &s.color.x);
                ImGui::DragFloat ("Tiling", &s.tilingFactor, 0.1f, 0.0f, 100.0f);
                ImGui::TextDisabled(s.texture ? "Texture: set" : "Texture: none");
            }
        }

        if (entity.has<Velocity2D>())
        {
            auto& v = entity.get<Velocity2D>();
            if (ImGui::CollapsingHeader("Velocity2D", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat2("Linear",  &v.linear.x, 0.05f);
                ImGui::DragFloat ("Angular", &v.angular,  1.0f);
            }
        }

        if (entity.has<SpriteAnimation>())
        {
            auto& a = entity.get<SpriteAnimation>();
            if (ImGui::CollapsingHeader("Sprite Animation", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Frame %u / %zu", a.currentFrame + 1u, a.frames.size());
                ImGui::DragFloat("Frame Duration", &a.frameDuration, 0.01f, 0.0f, 10.0f);
                ImGui::Checkbox("Looping", &a.looping);
                ImGui::Checkbox("Playing", &a.playing);
            }
        }

        // ── Physics ─────────────────────────────────────────────────

        if (entity.has<RigidBody2D>())
        {
            auto& rb = entity.get<RigidBody2D>();
            if (ImGui::CollapsingHeader("Rigid Body 2D", ImGuiTreeNodeFlags_DefaultOpen))
            {
                char const* const kinds[] = { "Static", "Dynamic", "Kinematic" };
                int kind = static_cast<int>(rb.type);
                if (ImGui::Combo("Body Type", &kind, kinds, 3))
                    rb.type = static_cast<RigidBody2D::BodyType>(kind);

                ImGui::DragFloat ("Mass",          &rb.mass,         0.05f, 0.0f, 1000.0f);
                ImGui::DragFloat ("Gravity Scale", &rb.gravityScale, 0.05f, -10.0f, 10.0f);
                ImGui::Checkbox  ("Fixed Rotation", &rb.fixedRotation);
            }
        }

        // Shared material/trigger editor for any collider.
        auto material = [](float& density, float& friction, float& restitution, bool& isTrigger)
        {
            ImGui::DragFloat("Density",     &density,     0.05f, 0.0f, 100.0f);
            ImGui::DragFloat("Friction",    &friction,    0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f);
            ImGui::Checkbox ("Is Trigger",  &isTrigger);
        };

        if (entity.has<BoxCollider2D>())
        {
            auto& c = entity.get<BoxCollider2D>();
            if (ImGui::CollapsingHeader("Box Collider 2D", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat2("Size##box",   &c.size.x,   0.05f, 0.01f, 100.0f);
                ImGui::DragFloat2("Offset##box", &c.offset.x, 0.05f);
                material(c.density, c.friction, c.restitution, c.isTrigger);
            }
        }

        if (entity.has<CircleCollider2D>())
        {
            auto& c = entity.get<CircleCollider2D>();
            if (ImGui::CollapsingHeader("Circle Collider 2D", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat ("Radius##cir", &c.radius,   0.05f, 0.01f, 100.0f);
                ImGui::DragFloat2("Offset##cir", &c.offset.x, 0.05f);
                material(c.density, c.friction, c.restitution, c.isTrigger);
            }
        }

        if (entity.has<PolygonCollider2D>())
        {
            auto& c = entity.get<PolygonCollider2D>();
            if (ImGui::CollapsingHeader("Polygon Collider 2D", ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (std::size_t i = 0; i < c.points.size(); ++i)
                {
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::DragFloat2("Point", &c.points[i].x, 0.05f);
                    ImGui::PopID();
                }
                material(c.density, c.friction, c.restitution, c.isTrigger);
            }
        }

        // ── Add Component ───────────────────────────────────────────

        ImGui::Separator();
        if (ImGui::Button("Add Component"))
            ImGui::OpenPopup("AddComponent");

        if (ImGui::BeginPopup("AddComponent"))
        {
            if (!entity.has<Transform>()         && ImGui::MenuItem("Transform"))           entity.add<Transform>();
            if (!entity.has<SpriteRenderer>()    && ImGui::MenuItem("Sprite Renderer"))     entity.add<SpriteRenderer>();
            if (!entity.has<Velocity2D>()        && ImGui::MenuItem("Velocity2D"))          entity.add<Velocity2D>();
            if (!entity.has<RigidBody2D>()       && ImGui::MenuItem("Rigid Body 2D"))       entity.add<RigidBody2D>();
            if (!entity.has<BoxCollider2D>()     && ImGui::MenuItem("Box Collider 2D"))     entity.add<BoxCollider2D>();
            if (!entity.has<CircleCollider2D>()  && ImGui::MenuItem("Circle Collider 2D"))  entity.add<CircleCollider2D>();
            ImGui::EndPopup();
        }
    }
}

EditorLayer::EditorLayer(Engine::Scene::Project project)
    : ILayer    { "Ω::EditorLayer" }
    , m_project { std::move(project) }
{}

void EditorLayer::onAttach()
{
    std::println("[Ω::EditorLayer] attached");

    // ── Batch renderer ─────────────────────────────────────────
    // Required GPU infrastructure (shader, white pixel, VBO/IBO/VAO).
    // NOT a texture asset -- this must run before any draw call.
    auto initResult = Engine::Renderer::Renderer2D::init();
    if (!initResult)
    {
        std::println(std::cerr, "[Ω::EditorLayer] Renderer2D init failed: {}", initResult.error().message);
        return;
    }

    // ── Framebuffer (FBO) ──────────────────────────────────────
    auto fbResult = Engine::Renderer::Framebuffer::create(1280, 720);
    if (fbResult)
        m_framebuffer.emplace(std::move(*fbResult));
    else
        std::println(std::cerr, "[Ω::EditorLayer] framebuffer failed: {}", fbResult.error().message);

    // ── Camera ─────────────────────────────────────────────────
    // size=2.0 means the view spans [-2,+2] vertically — enough
    // to see the test quads spread around the origin.
    m_camera.emplace(16.0f / 9.0f, 2.0f);

    // Console feedback when physics bodies collide during Play.
    {
        auto nameOf = [](Engine::ECS::Entity e) -> std::string
        {
            return (e.valid() && e.has<Engine::ECS::NameComponent>())
                 ? e.get<Engine::ECS::NameComponent>().name : std::string { "?" };
        };
        Engine::Core::Application::get().eventBus().subscribe<Engine::Physics::CollisionEnterEvent>(
            [nameOf](Engine::Physics::CollisionEnterEvent const& e)
            {
                std::println("[Ω::Physics] collision  {} <-> {}", nameOf(e.a), nameOf(e.b));
            });
    }

    // ── ECS world ──────────────────────────────────────
    // The editor edits a scene from the OPEN PROJECT. We open its startup
    // scene; its entities are pure DATA loaded from the project's
    // scenes/<name>.json in the onEnter hook below. (Switching to other
    // scenes in the project is the next step, P3.)
    // One world per scene the project declares (Scene menu switches them).
    // The setup is IDEMPOTENT: it only loads + wires systems the FIRST
    // time a world is entered (systemCount == 0). Switching away does NOT
    // clear the world, so returning to a scene preserves your in-editor
    // edits (re-entry skips the reload).
    auto setup = [this](Engine::Scene::World& w)
    {
        if (w.systemCount() > 0)
            return;     // already loaded -- keep edits

        // Systems are engine CODE (not serialized). They only TICK in Play
        // mode (see onUpdate); Edit mode stays static for authoring.
        w.addSystem<Engine::Systems::InterpolationSystem>();   // FIRST (snapshot before movers)
        w.addSystem<Engine::Systems::MovementSystem>();
        w.addSystem<Engine::Systems::AnimationSystem>();
        w.addSystem<Engine::Physics::PhysicsSystem>(Engine::Core::Application::get().eventBus());

        // Entities are pure DATA from the project's scenes/<name>.json.
        auto& assets    = Engine::Core::Application::get().assets();
        auto const file = "scenes/" + w.name() + ".json";
        if (auto r = Engine::Scene::SceneSerializer::load(w, file, assets); !r)
            std::println(std::cerr, "[Ω::EditorLayer] could not load '{}': {}",
                         file, r.error().message);
    };

    for (auto const& name : m_project.scenes())
        m_sceneManager.create(name).setOnEnter(setup);

    m_sceneManager.switchTo(m_project.startupScene());

    logConsole("opened project '" + m_project.name() + "'  (" +
               std::to_string(m_project.scenes().size()) + " scenes)");
    logConsole(ICON_FA_LIST " scene -> " + m_project.startupScene());
}

void EditorLayer::onDetach()
{
    m_framebuffer.reset();
    m_camera.reset();

    // Shutdown the batch renderer (frees its internal GPU resources).
    Engine::Renderer::Renderer2D::shutdown();

    std::println("[Ω::EditorLayer] detached");
}

void EditorLayer::onUpdate(float dt)
{
    using Engine::Core::Key;
    using Input = Engine::Core::Input;

    // Always pump the manager so the scene LOADS (the deferred initial
    // switch runs enter()) and actions route -- but only TICK the world's
    // systems in Play mode, so Edit mode stays static for authoring.
    m_sceneManager.onUpdate(dt, m_playing);

    // Global editor shortcuts (work regardless of viewport focus):
    //   Ctrl+S = save scene, Ctrl+O = load scene, Ctrl+P = play/stop.
    if (Input::isKeyDown(Key::LeftControl) && Input::wasKeyPressed(Key::P))
        togglePlay();

    if (Input::isKeyDown(Key::LeftControl))
    {
        if (Input::wasKeyPressed(Key::S)) saveScene();
        if (Input::wasKeyPressed(Key::O)) loadScene();
    }

    if (!m_camera || !m_viewportHovered) return;

    // ── Camera controls ─────────────────────────────────────────
    // WASD = pan, Q/E = rotate, mouse wheel = zoom. Driven by the
    // engine's Input service. Gated on m_viewportHovered above so typing
    // in other panels doesn't move the camera.

    constexpr float panSpeed    = 2.0f;     // world units per second
    constexpr float rotateSpeed = 90.0f;    // degrees per second

    auto position = m_camera->position();
    auto rotation = m_camera->rotation();

    // Pan — move in camera-local axes so WASD feels correct
    // even when the camera is rotated.
    float const rad = glm::radians(rotation);
    float const c   = std::cos(rad);
    float const s   = std::sin(rad);

    float const step = panSpeed * dt / m_camera->zoom();    // zoom-compensated

    if (Input::isKeyDown(Key::W))  { position.x -= s * step; position.y += c * step; }
    if (Input::isKeyDown(Key::S))  { position.x += s * step; position.y -= c * step; }
    if (Input::isKeyDown(Key::A))  { position.x -= c * step; position.y -= s * step; }
    if (Input::isKeyDown(Key::D))  { position.x += c * step; position.y += s * step; }

    if (Input::isKeyDown(Key::Q))  rotation += rotateSpeed * dt;
    if (Input::isKeyDown(Key::E))  rotation -= rotateSpeed * dt;

    // Scroll wheel zoom is handled in onImGuiRender() because
    // ImGui::GetIO().MouseWheel isn't populated until NewFrame(),
    // which runs after onUpdate.

    m_camera->setPosition(position);
    m_camera->setRotation(rotation);
}

void EditorLayer::onRender(float alpha)
{
    if (!m_framebuffer || !m_camera) return;

    // Reset per-frame stats so the Console panel shows this frame's numbers.
    Engine::Renderer::Renderer2D::resetStats();

    // Bind the FBO — all draw calls go into its color texture.
    m_framebuffer->bind();
    Engine::Renderer::RenderCommand::setClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    Engine::Renderer::RenderCommand::clear();

    // ── ECS render pass ─────────────────────────────────────────
    // Hand the ACTIVE world's registry to the RenderSystem. It opens
    // its own batch, iterates every entity with Transform +
    // SpriteRenderer, draws each, and flushes. The EditorLayer no
    // longer owns a Registry — it asks the SceneManager for the active
    // world. The FBO is still bound here, so these quads land in the
    // same off-screen target as the demo batch.
    if (auto* world = m_sceneManager.active())
    {
        Engine::Systems::RenderSystem::render(world->registry(), *m_camera, alpha);

        // Collider wireframe overlay (authoring aid). Drawn after the
        // sprites, into the same FBO, so it sits on top.
        if (m_showColliders)
            Engine::Systems::RenderSystem::renderColliders(world->registry(), *m_camera, alpha);
    }

    // F2 -> screenshot the GAME VIEW (the viewport FBO, no editor chrome),
    // captured while the FBO is still bound.
    if (Engine::Core::Input::wasKeyPressed(Engine::Core::Key::F2))
    {
        auto const path = Engine::Renderer::Screenshot::timestamped();
        if (auto r = Engine::Renderer::Screenshot::capture(
                path, 0, 0,
                static_cast<int>(m_framebuffer->width()),
                static_cast<int>(m_framebuffer->height())); !r)
            std::println(std::cerr, "[Ω::EditorLayer] screenshot failed: {}", r.error().message);
        else
            logConsole(ICON_FA_CAMERA " screenshot -> " + path.string());
    }

    m_framebuffer->unbind();
}

void EditorLayer::onImGuiRender()
{
    // Ω::Fullscreen DockSpace host ────────────────────────────────
    auto const* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    constexpr ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
    ImGui::Begin("##Ω_DockSpaceHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    ImGui::DockSpace(ImGui::GetID("ΩmegaEngineDockSpace"));

    // Ω::Menu bar ─────────────────────────────────────────────────
    if(ImGui::BeginMenuBar())
    {
        if(ImGui::BeginMenu("File"))
        {
            if(ImGui::MenuItem("Save Scene", "Ctrl+S")) saveScene();
            if(ImGui::MenuItem("Load Scene", "Ctrl+O")) loadScene();
            ImGui::Separator();
            if(ImGui::MenuItem("Exit", "Alt+F4"))
                Engine::Core::Application::get().quit();
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Viewport", nullptr, &m_showViewport);
            ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
            ImGui::MenuItem("Hierarchy", nullptr, &m_showHierarchy);
            ImGui::MenuItem("Console", nullptr, &m_showConsole);
            ImGui::Separator();
            ImGui::MenuItem("Colliders", nullptr, &m_showColliders);
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Scene"))
        {
            // One entry per scene in the open project; the active scene is
            // ticked. Switching is deferred to the frame boundary by the
            // SceneManager (safe mid-frame).
            auto const* active = m_sceneManager.active();
            for(auto const& name : m_project.scenes())
            {
                bool const isActive = active && active->name() == name;
                if(ImGui::MenuItem(name.c_str(), nullptr, isActive) && !isActive)
                    switchScene(name);
            }
            ImGui::EndMenu();
        }

        // Play / Stop toggle (edit vs play-in-editor). Green = will play,
        // red = currently playing (click to stop + restore).
        ImGui::PushStyleColor(ImGuiCol_Button, m_playing ? ImVec4 { 0.70f, 0.20f, 0.20f, 1.0f }
                                                         : ImVec4 { 0.20f, 0.55f, 0.30f, 1.0f });
        if(ImGui::Button(m_playing ? ICON_FA_STOP " Stop##play" : ICON_FA_PLAY " Play##play", { 78.0f, 0.0f }))
            togglePlay();
        ImGui::PopStyleColor();
        if(m_playing)
        {
            ImGui::SameLine();
            ImGui::TextColored({ 0.9f, 0.6f, 0.3f, 1.0f }, "PLAYING");
        }

        ImVec2 const tl = ImGui::GetWindowPos();
        ImVec2 const br = { tl.x + ImGui::GetWindowWidth(), tl.y + g_titlebarHeight };
        auto* dl = ImGui::GetWindowDrawList();

        std::string const title = "ΩmegaEngine Editor — " + m_project.name();
        ImVec2 const textSize = ImGui::CalcTextSize(title.c_str());
        float  const textX = tl.x + (br.x - tl.x - textSize.x) * 0.5f;
        float  const textY = tl.y + (g_titlebarHeight - textSize.y) * 0.5f;
        dl->AddText({ textX, textY }, IM_COL32(180, 178, 200, 255), title.c_str());

#ifdef _WIN32
    // Ω::Window control buttons (right-aligned) ──────────────
        float frameH = ImGui::GetFrameHeight();
        float btnW = frameH * 1.5f;
        float windowW = ImGui::GetWindowWidth();

        ImGui::SameLine(windowW - 3 * btnW);

        ImGui::PushStyleColor(ImGuiCol_Button, { 0.0f, 0.0f, 0.0f, 0.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.3f, 0.3f, 0.3f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.15f, 0.15f, 0.15f, 1.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

        auto& win = Engine::Core::Application::get().window();
        if(ImGui::Button(ICON_FA_MINUS "##ΩMin", { btnW, frameH }))
            win.minimize();

        ImGui::SameLine(0, 0);

        if(ImGui::Button(win.isMaximized() ? ICON_FA_WINDOW_RESTORE "##ΩMax" : ICON_FA_WINDOW_MAXIMIZE "##ΩMax", { btnW, frameH }))
        {
            if(win.isMaximized())  win.restore();
            else                   win.maximize();
        }

        ImGui::SameLine(0, 0);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.86f, 0.2f, 0.2f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.70f, 0.1f, 0.1f, 1.0f });
        if(ImGui::Button(ICON_FA_XMARK "##ΩClose", { btnW, frameH }))
            win.close();
        ImGui::PopStyleColor(2);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
#endif

        ImGui::EndMenuBar();
    }

    // Ω::Panels ───────────────────────────────────────────────────
    if(m_showViewport)
    {
        // Zero padding so the rendered image fills the panel edge-to-edge.
        // NoScrollWithMouse prevents ImGui from eating the scroll wheel —
        // we use it for camera zoom instead.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport", &m_showViewport,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        if (m_framebuffer)
        {
            auto const size = ImGui::GetContentRegionAvail();

            if (size.x > 0 && size.y > 0)
            {
                auto const w = static_cast<std::uint32_t>(size.x);
                auto const h = static_cast<std::uint32_t>(size.y);

                if (w != m_framebuffer->width() || h != m_framebuffer->height())
                {
                    m_framebuffer->resize(w, h);

                    if (m_camera)
                        m_camera->setAspectRatio(
                            static_cast<float>(w) / static_cast<float>(h)
                        );
                }

                auto const texId = static_cast<ImTextureID>(m_framebuffer->colorAttachment());
                ImGui::Image(texId, size, ImVec2(0, 1), ImVec2(1, 0));
            }
        }

        m_viewportHovered = ImGui::IsWindowHovered();

        // Scroll wheel zoom — lives here (not in onUpdate) because
        // ImGui::GetIO().MouseWheel is only valid after NewFrame().
        if (m_viewportHovered && m_camera)
        {
            constexpr float zoomSpeed = 0.15f;
            float const wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
            {
                float const zoom = std::clamp(
                    m_camera->zoom() * (1.0f + wheel * zoomSpeed),
                    0.1f, 50.0f
                );
                m_camera->setZoom(zoom);
            }
        }

        ImGui::End();
    }

    if(m_showHierarchy)
    {
        ImGui::Begin("Scene Hierarchy", &m_showHierarchy);

        if (auto* world = m_sceneManager.active())
        {
            ImGui::TextDisabled("%s", world->name().c_str());
            ImGui::Separator();

            // List every entity in the active world. PushID(index) keeps
            // ImGui's selectable IDs unique even when names repeat.
            int index = 0;
            world->registry().eachEntity([&](Engine::ECS::Entity e)
            {
                ImGui::PushID(index++);

                std::string const label = e.has<Engine::ECS::NameComponent>()
                    ? e.get<Engine::ECS::NameComponent>().name
                    : std::string { "Entity" };

                if (ImGui::Selectable(label.c_str(), e == m_selected))
                    m_selected = e;

                ImGui::PopID();
            });

            // Click empty space in the panel to deselect.
            if (ImGui::IsWindowHovered()
                && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                && !ImGui::IsAnyItemHovered())
            {
                m_selected = {};
            }
        }
        else
        {
            ImGui::TextDisabled("(no active scene)");
        }

        ImGui::End();
    }

    if(m_showInspector)
    {
        ImGui::Begin("Inspector", &m_showInspector);

        // Guard with valid(): a scene rebuild destroys entities and
        // leaves m_selected dangling -- valid() catches that.
        if (m_selected.valid())
            drawInspector(m_selected);
        else
            ImGui::TextDisabled("No entity selected");

        ImGui::End();
    }

    if(m_showConsole)
    {
        ImGui::Begin("Console", &m_showConsole);

        // ── Header: status + hotkey reference + per-frame stats ──
        ImGui::TextColored({ 0.3f, 0.9f, 0.5f, 1.0f },
                           "[Ω] OmegaEngine started successfully");
        ImGui::TextColored({ 0.55f, 0.55f, 0.65f, 1.0f },
                           ICON_FA_PLAY " Ctrl+P play/stop   "
                           ICON_FA_FLOPPY_DISK " Ctrl+S save   "
                           ICON_FA_FOLDER_OPEN " Ctrl+O reload   "
                           ICON_FA_CAMERA " F2 screenshot");

        auto const stats = Engine::Renderer::Renderer2D::stats();
        ImGui::TextColored({ 0.5f, 0.7f, 0.9f, 1.0f },
                           "[Ω] Draw calls: %u  |  Quads: %u   ·   60Hz (%.2fms)",
                           stats.drawCalls, stats.quadCount, 1000.0f / 60.0f);

        // ── Scrolling action log ────────────────────────────────
        ImGui::Separator();
        ImGui::BeginChild("##consolelog", { 0.0f, 0.0f }, false,
                          ImGuiWindowFlags_HorizontalScrollbar);
        for(auto const& line : m_consoleLog)
            ImGui::TextUnformatted(line.c_str());
        if(m_consoleScrollToBottom)
        {
            ImGui::SetScrollHereY(1.0f);
            m_consoleScrollToBottom = false;
        }
        ImGui::EndChild();

        ImGui::End();
    }

    ImGui::End(); // DockSpaceHost

    // Ω::Update titlebar state for WndProc ───────────────────────
#ifdef _WIN32
    g_imguiWantsInput = ImGui::IsAnyItemHovered()
        || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup);
    g_titlebarHeight = static_cast<int>(
        ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y * 2
        );
#endif
}

void EditorLayer::saveScene()
{
    auto* world = m_sceneManager.active();
    if (!world) return;

    // Save to "scenes/<world>.json" -- the same file onEnter loads from.
    auto const result = Engine::Scene::SceneSerializer::save(*world, "scenes/" + world->name() + ".json");
    if (!result)   // the serializer logs success; we only flag failures
        std::println(std::cerr, "[Ω::EditorLayer] save failed: {}", result.error().message);
    else
        logConsole(ICON_FA_FLOPPY_DISK " saved scene '" + world->name() + "'");
}

void EditorLayer::loadScene()
{
    auto* world = m_sceneManager.active();
    if (!world) return;

    m_selected = {};   // old entities are about to be destroyed

    auto const result = Engine::Scene::SceneSerializer::load(
        *world, "scenes/" + world->name() + ".json", Engine::Core::Application::get().assets());
    if (!result)
        std::println(std::cerr, "[Ω::EditorLayer] load failed: {}", result.error().message);
    else
        logConsole(ICON_FA_FOLDER_OPEN " reloaded scene '" + world->name() + "'");
}

void EditorLayer::togglePlay()
{
    auto* world = m_sceneManager.active();
    if (!world) return;

    // A throwaway snapshot of the PRE-play (possibly edited) state. We
    // reuse the scene serializer rather than the committed scene file, so
    // Play captures unsaved edits and Stop restores exactly them.
    auto const snapshot = std::filesystem::temp_directory_path() / "omega_editor_play_snapshot.json";

    if (!m_playing)
    {
        // Edit -> Play: snapshot, then let the systems run.
        if (auto const r = Engine::Scene::SceneSerializer::save(*world, snapshot); !r)
        {
            std::println(std::cerr, "[Ω::EditorLayer] play snapshot failed: {}", r.error().message);
            return;     // don't enter Play if we couldn't capture a restore point
        }
        m_playing = true;
        logConsole(ICON_FA_PLAY " play  (scene '" + world->name() + "')");
    }
    else
    {
        // Play -> Edit: restore the snapshot, discarding the simulation.
        m_playing  = false;
        m_selected = {};    // entities are recreated by load(); old handle is stale
        if (auto const r = Engine::Scene::SceneSerializer::load(
                *world, snapshot, Engine::Core::Application::get().assets()); !r)
            std::println(std::cerr, "[Ω::EditorLayer] restore failed: {}", r.error().message);
        logConsole(ICON_FA_STOP " stop  (restored)");
    }
}

void EditorLayer::switchScene(std::string name)
{
    if (m_playing)          // leaving Play; restore first so we don't carry sim state
        togglePlay();

    m_selected = {};        // old scene's entities go away
    m_sceneManager.switchTo(name);
    logConsole(ICON_FA_LIST " scene -> " + name);
}

void EditorLayer::logConsole(std::string message)
{
    m_consoleLog.push_back(std::move(message));

    constexpr std::size_t maxLines = 200;
    if (m_consoleLog.size() > maxLines)
        m_consoleLog.erase(m_consoleLog.begin(),
                           m_consoleLog.begin() + (m_consoleLog.size() - maxLines));

    m_consoleScrollToBottom = true;
}