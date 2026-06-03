module;

#include "imgui.h"
#include "ImGuizmo.h"                          // after imgui.h -- depends on it
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"        // translate / rotate / scale
#include "glm/gtc/type_ptr.hpp"                // value_ptr (mat4 -> float*)
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
    // ── Duplicate: copy every known component ─────────────
    // There is no C++ reflection, so a clone mirrors the inspector's
    // component set by hand. Add new components here when the engine
    // grows them. add<T>(const T&) copy-constructs into the new entity.
    void cloneInto(Engine::ECS::Entity src, Engine::ECS::Entity dst)
    {
        using namespace Engine::ECS;
        if (src.has<NameComponent>())     dst.add<NameComponent>(src.get<NameComponent>());
        if (src.has<Transform>())         dst.add<Transform>(src.get<Transform>());
        if (src.has<SpriteRenderer>())    dst.add<SpriteRenderer>(src.get<SpriteRenderer>());
        if (src.has<Velocity2D>())        dst.add<Velocity2D>(src.get<Velocity2D>());
        if (src.has<SpriteAnimation>())   dst.add<SpriteAnimation>(src.get<SpriteAnimation>());
        if (src.has<RigidBody2D>())       dst.add<RigidBody2D>(src.get<RigidBody2D>());
        if (src.has<BoxCollider2D>())     dst.add<BoxCollider2D>(src.get<BoxCollider2D>());
        if (src.has<CircleCollider2D>())  dst.add<CircleCollider2D>(src.get<CircleCollider2D>());
        if (src.has<PolygonCollider2D>()) dst.add<PolygonCollider2D>(src.get<PolygonCollider2D>());
    }

    // ── Viewport math: screen -> world + picking  ──────────
    // The viewport image occupies the rect [mn, mx] in ImGui SCREEN
    // coordinates. A click maps back through the camera's view-projection
    // (vp) to world space. Y is flipped: screen grows DOWN, but the FBO is
    // drawn flipped so world-up is screen-up. (The gizmo itself is handled
    // by ImGuizmo, which takes the camera's separate view/projection.)

    glm::vec2 screenToWorld(ImVec2 s, glm::mat4 const& vp, ImVec2 mn, ImVec2 mx)
    {
        float const u = (s.x - mn.x) / (mx.x - mn.x);
        float const v = (s.y - mn.y) / (mx.y - mn.y);
        glm::vec4 const world =
            glm::inverse(vp) * glm::vec4 { u * 2.0f - 1.0f, 1.0f - v * 2.0f, 0.0f, 1.0f };
        return { world.x, world.y };
    }

    // Point-in-oriented-box test for picking. Transform is center +
    // rotation (deg) + full-size scale. Rotate the point into the box's
    // local frame, then compare against half-extents.
    bool hitTest(Engine::ECS::Transform const& t, glm::vec2 p)
    {
        glm::vec2 const d   = p - t.position;
        float     const rad = glm::radians(-t.rotation);
        float     const c   = std::cos(rad);
        float     const s   = std::sin(rad);
        glm::vec2 const local { d.x * c - d.y * s, d.x * s + d.y * c };
        glm::vec2 const half = t.scale * 0.5f;
        return std::abs(local.x) <= half.x && std::abs(local.y) <= half.y;
    }

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
    for (auto const& name : m_project.scenes())
        m_sceneManager.create(name).setOnEnter(
            [this](Engine::Scene::World& w) { setupWorld(w); });

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

    // Apply a deferred scene-world removal once the switch-away has taken
    // effect (delete/rename). We never free the world that was
    // active THIS frame -- only after the SceneManager has activated the
    // replacement scene.
    if (m_pendingSceneRemoval)
    {
        auto const* active = m_sceneManager.active();
        if (active && active->name() != *m_pendingSceneRemoval)
        {
            m_sceneManager.remove(*m_pendingSceneRemoval);
            m_pendingSceneRemoval.reset();
        }
    }

    // NOTE: edge-triggered shortcuts (Ctrl+S/O/P/D, Del, 1/2/3) are handled
    // in handleShortcuts() (per render frame), NOT here -- onUpdate runs on
    // the fixed-timestep tick, which is skipped on fast frames, so taps were
    // being missed. Only the CONTINUOUS camera controls below stay here,
    // where integrating over fixedDt is correct.
    bool const ctrl = Input::isKeyDown(Key::LeftControl);

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

    // Skip WASD pan while Ctrl is held so Ctrl+S / Ctrl+D (save / duplicate)
    // don't also drag the camera.
    if (!ctrl)
    {
        if (Input::isKeyDown(Key::W))  { position.x -= s * step; position.y += c * step; }
        if (Input::isKeyDown(Key::S))  { position.x += s * step; position.y -= c * step; }
        if (Input::isKeyDown(Key::A))  { position.x -= c * step; position.y -= s * step; }
        if (Input::isKeyDown(Key::D))  { position.x += c * step; position.y += s * step; }
    }

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
        // Tilemaps are the background layer -- drawn first, sprites on top.
        Engine::Systems::RenderSystem::renderTilemaps(world->registry(), *m_camera);

        // Cell-grid overlay (authoring aid), over the tiles, under sprites.
        if (m_showGrid)
            Engine::Systems::RenderSystem::renderTilemapGrid(world->registry(), *m_camera);

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
    // ImGuizmo is stateful per-frame: reset it right after ImGui::NewFrame
    // (which the ImGuiLayer ran before this) and before any Manipulate call.
    ImGuizmo::BeginFrame();

    // Edge-triggered keyboard shortcuts, polled once per render frame here
    // (see handleShortcuts() for why not in onUpdate).
    handleShortcuts();

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
            ImGui::MenuItem("Tilemap Grid", nullptr, &m_showGrid);
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

            // ── Scene authoring  ────────────────────────────
            // Popups can't be opened from inside a menu (ImGui closes the
            // menu first), so these only RAISE a request flag; the modal
            // is opened + drawn after the dockspace below.
            ImGui::Separator();
            if(ImGui::MenuItem(ICON_FA_PLUS " New Scene..."))
                m_openNewScenePopup = true;

            if(active)
            {
                if(ImGui::MenuItem(ICON_FA_PEN " Rename Scene..."))
                {
                    m_renameSceneFrom = active->name();
                    std::snprintf(m_sceneNameBuf, sizeof(m_sceneNameBuf), "%s",
                                  active->name().c_str());
                    m_openRenameScenePopup = true;
                }

                bool const isStartup = active->name() == m_project.startupScene();
                if(ImGui::MenuItem(ICON_FA_STAR " Set as Startup", nullptr, isStartup, !isStartup))
                    setStartupScene(active->name());

                bool const canDelete = m_project.scenes().size() > 1;
                if(ImGui::MenuItem(ICON_FA_TRASH " Delete Scene", nullptr, false, canDelete))
                    deleteScene(active->name());
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

        // The viewport image's on-screen rect -- needed to map mouse <->
        // world for picking + the gizmo. Captured right after
        // the ImGui::Image() draw below.
        ImVec2 imageMin {};
        ImVec2 imageMax {};
        bool   haveImage { false };

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

                imageMin  = ImGui::GetItemRectMin();
                imageMax  = ImGui::GetItemRectMax();
                haveImage = true;
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

        // ── Viewport gizmo (ImGuizmo) + picking  ───────
        if (haveImage && m_camera)
        if (auto* world = m_sceneManager.active())
        {
            auto& reg = world->registry();

            // Mode toolbar overlay (top-left of the viewport). Mirrors the
            // 1/2/3 hotkeys; the active mode is highlighted. Drawn ON TOP of
            // the image via an absolute cursor position.
            ImGui::SetCursorScreenPos({ imageMin.x + 8.0f, imageMin.y + 8.0f });
            for (auto const& [label, mode] : { std::pair { "Move (1)", 0 },
                                               std::pair { "Rotate (2)", 1 },
                                               std::pair { "Scale (3)", 2 } })
            {
                bool const on = m_gizmoOp == mode;
                if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 { 0.20f, 0.45f, 0.75f, 1.0f });
                if (ImGui::Button(label)) m_gizmoOp = mode;
                if (on) ImGui::PopStyleColor();
                ImGui::SameLine();
            }
            ImGui::NewLine();

            // ImGuizmo draws + manipulates the selected entity's transform.
            // It works in 2D via orthographic mode: feed it the camera's
            // separate view/projection and a 4x4 model built from Transform,
            // then decompose the manipulated matrix back to our 2D fields.
            bool overGizmo = false;
            if (m_selected.valid() && m_selected.has<Engine::ECS::Transform>())
            {
                auto& t = m_selected.get<Engine::ECS::Transform>();

                ImGuizmo::SetOrthographic(true);
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(imageMin.x, imageMin.y,
                                  imageMax.x - imageMin.x, imageMax.y - imageMin.y);

                glm::mat4 model =
                    glm::translate(glm::mat4 { 1.0f }, glm::vec3 { t.position.x, t.position.y, 0.0f })
                  * glm::rotate   (glm::mat4 { 1.0f }, glm::radians(t.rotation), glm::vec3 { 0.0f, 0.0f, 1.0f })
                  * glm::scale    (glm::mat4 { 1.0f }, glm::vec3 { t.scale.x, t.scale.y, 1.0f });

                ImGuizmo::OPERATION const op =
                    (m_gizmoOp == 1) ? ImGuizmo::ROTATE :
                    (m_gizmoOp == 2) ? ImGuizmo::SCALE  : ImGuizmo::TRANSLATE;

                if (ImGuizmo::Manipulate(
                        glm::value_ptr(m_camera->view()),
                        glm::value_ptr(m_camera->projection()),
                        op, ImGuizmo::LOCAL,
                        glm::value_ptr(model)))
                {
                    // Decompose back to our 2D Transform. ImGuizmo returns
                    // rotation in DEGREES (matching Transform.rotation); for
                    // 2D we keep only the Z angle and the XY pos/scale.
                    float translation[3], rotation[3], scale[3];
                    ImGuizmo::DecomposeMatrixToComponents(
                        glm::value_ptr(model), translation, rotation, scale);
                    t.position = { translation[0], translation[1] };
                    t.rotation = rotation[2];
                    t.scale    = { scale[0], scale[1] };
                }

                overGizmo = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
            }

            // Picking: a click NOT on the gizmo selects the front-most
            // sprite under the cursor (or deselects on empty space).
            // eachEntity keeps EnTT iteration out of this TU.
            if (m_viewportHovered && !overGizmo
                && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                glm::vec2 const wp = screenToWorld(
                    ImGui::GetMousePos(), m_camera->viewProjection(), imageMin, imageMax);
                Engine::ECS::Entity hit;
                reg.eachEntity([&](Engine::ECS::Entity e)
                {
                    if (e.has<Engine::ECS::Transform>() && e.has<Engine::ECS::SpriteRenderer>()
                        && hitTest(e.get<Engine::ECS::Transform>(), wp))
                        hit = e;   // last match = front-most in draw order
                });
                m_selected = hit;  // empty handle -> deselect
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

            // Create buttons. The right-click context menu on the
            // panel background offers the same, plus Del / Ctrl+D shortcuts.
            if (ImGui::SmallButton(ICON_FA_PLUS " Empty"))   createEntity("Entity");
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_PLUS " Sprite"))  createSpriteEntity();
            ImGui::Separator();

            // Snapshot the entity handles BEFORE iterating the UI, so a
            // create/delete/duplicate triggered by a row's context menu
            // doesn't mutate the registry storage we're walking.
            std::vector<Engine::ECS::Entity> entities;
            world->registry().eachEntity([&](Engine::ECS::Entity e) { entities.push_back(e); });

            int index = 0;
            for (auto e : entities)
            {
                ImGui::PushID(index++);

                std::string const label = e.has<Engine::ECS::NameComponent>()
                    ? e.get<Engine::ECS::NameComponent>().name
                    : std::string { "Entity" };

                if (ImGui::Selectable(label.c_str(), e == m_selected))
                    m_selected = e;

                // Right-click a row -> per-entity actions (selects it first).
                if (ImGui::BeginPopupContextItem())
                {
                    m_selected = e;
                    if (ImGui::MenuItem(ICON_FA_CLONE " Duplicate", "Ctrl+D")) duplicateSelected();
                    if (ImGui::MenuItem(ICON_FA_TRASH " Delete",    "Del"))    deleteSelected();
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            // Right-click empty panel background -> create menu.
            if (ImGui::BeginPopupContextWindow("##hierctx",
                    ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                if (ImGui::MenuItem(ICON_FA_CUBE " Create Empty"))  createEntity("Entity");
                if (ImGui::MenuItem(ICON_FA_CUBE " Create Sprite")) createSpriteEntity();
                ImGui::EndPopup();
            }

            // Left-click empty space in the panel to deselect.
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
        ImGui::TextColored({ 0.55f, 0.55f, 0.65f, 1.0f },
                           ICON_FA_CLONE " Ctrl+D duplicate   "
                           ICON_FA_TRASH " Del delete   "
                           "1/2/3 gizmo move/rotate/scale");

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

    // ── Scene name modals (New / Rename) ───────────────────
    // Opened here (outside the menu) from the request flags set above.
    if (m_openNewScenePopup)    { ImGui::OpenPopup("New Scene");    m_sceneNameBuf[0] = '\0'; m_openNewScenePopup    = false; }
    if (m_openRenameScenePopup) { ImGui::OpenPopup("Rename Scene");                            m_openRenameScenePopup = false; }

    if (ImGui::BeginPopupModal("New Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("New scene name:");
        ImGui::SetNextItemWidth(260.0f);
        bool const enter = ImGui::InputText("##newscene", m_sceneNameBuf, sizeof(m_sceneNameBuf),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
        bool const ok     = ImGui::Button("Create") || enter;
        ImGui::SameLine();
        bool const cancel = ImGui::Button("Cancel");

        if (ok && m_sceneNameBuf[0] != '\0')
        {
            newScene(m_sceneNameBuf);
            ImGui::CloseCurrentPopup();
        }
        if (cancel) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Rename Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Rename '%s' to:", m_renameSceneFrom.c_str());
        ImGui::SetNextItemWidth(260.0f);
        bool const enter = ImGui::InputText("##renamescene", m_sceneNameBuf, sizeof(m_sceneNameBuf),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
        bool const ok     = ImGui::Button("Rename") || enter;
        ImGui::SameLine();
        bool const cancel = ImGui::Button("Cancel");

        if (ok && m_sceneNameBuf[0] != '\0' && m_renameSceneFrom != m_sceneNameBuf)
        {
            renameScene(m_renameSceneFrom, m_sceneNameBuf);
            ImGui::CloseCurrentPopup();
        }
        if (cancel) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
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

void EditorLayer::handleShortcuts()
{
    using Engine::Core::Key;
    using Input = Engine::Core::Input;

    // Polled once per RENDER frame (from onImGuiRender). Input::update() ran
    // at the top of this frame, so wasKeyPressed() edges are valid here and
    // never dropped by skipped fixed-timestep ticks.

    bool const ctrl = Input::isKeyDown(Key::LeftControl);

    if (ctrl && Input::wasKeyPressed(Key::P)) togglePlay();
    if (ctrl)
    {
        if (Input::wasKeyPressed(Key::S)) saveScene();
        if (Input::wasKeyPressed(Key::O)) loadScene();
        if (Input::wasKeyPressed(Key::D)) duplicateSelected();
    }

    // Entity/gizmo keys are suppressed while a text field is focused, so
    // typing a name doesn't delete the entity or flip the gizmo mode.
    if (!ImGui::GetIO().WantTextInput)
    {
        if (Input::wasKeyPressed(Key::Delete) && m_selected.valid())
            deleteSelected();

        // Gizmo mode: 1 = move, 2 = rotate, 3 = scale (number row, so it
        // doesn't clash with WASD camera pan).
        if (Input::wasKeyPressed(Key::D1)) m_gizmoOp = 0;
        if (Input::wasKeyPressed(Key::D2)) m_gizmoOp = 1;
        if (Input::wasKeyPressed(Key::D3)) m_gizmoOp = 2;
    }
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

// ─────────────────────────────────────────────────────────────────────
//  World wiring + entity authoring 
// ─────────────────────────────────────────────────────────────────────

void EditorLayer::setupWorld(Engine::Scene::World& world)
{
    if (world.systemCount() > 0)
        return;     // already wired -- keep in-editor edits on re-entry

    // Systems are engine CODE (not serialized). They only TICK in Play
    // mode (see onUpdate); Edit mode stays static for authoring.
    world.addSystem<Engine::Systems::InterpolationSystem>();   // FIRST (snapshot before movers)
    world.addSystem<Engine::Systems::MovementSystem>();
    world.addSystem<Engine::Systems::AnimationSystem>();
    world.addSystem<Engine::Physics::PhysicsSystem>(Engine::Core::Application::get().eventBus());

    // Entities are pure DATA from scenes/<name>.json. A brand-new scene
    // (just created in the editor) has no file yet -- that's fine, it
    // starts empty and the file is written on the first save.
    auto const file = "scenes/" + world.name() + ".json";
    if (std::filesystem::exists(file))
    {
        auto& assets = Engine::Core::Application::get().assets();
        if (auto r = Engine::Scene::SceneSerializer::load(world, file, assets); !r)
            std::println(std::cerr, "[Ω::EditorLayer] could not load '{}': {}",
                         file, r.error().message);
    }
}

Engine::ECS::Entity EditorLayer::createEntity(std::string name)
{
    auto* world = m_sceneManager.active();
    if (!world)
        return {};

    auto e = world->registry().create(name);     // adds NameComponent
    e.add<Engine::ECS::Transform>();             // every entity needs a place
    m_selected = e;
    logConsole(ICON_FA_CUBE " + entity '" + name + "'");
    return e;
}

Engine::ECS::Entity EditorLayer::createSpriteEntity()
{
    auto e = createEntity("Sprite");
    if (e.valid())
        e.add<Engine::ECS::SpriteRenderer>();    // white quad (color-only)
    return e;
}

void EditorLayer::deleteSelected()
{
    auto* world = m_sceneManager.active();
    if (!world || !m_selected.valid())
        return;

    std::string const name = m_selected.has<Engine::ECS::NameComponent>()
        ? m_selected.get<Engine::ECS::NameComponent>().name
        : std::string { "entity" };

    world->registry().destroy(m_selected);
    m_selected = {};
    logConsole(ICON_FA_TRASH " - entity '" + name + "'");
}

void EditorLayer::duplicateSelected()
{
    auto* world = m_sceneManager.active();
    if (!world || !m_selected.valid())
        return;

    auto dst = world->registry().create();
    cloneInto(m_selected, dst);

    if (dst.has<Engine::ECS::NameComponent>())
        dst.get<Engine::ECS::NameComponent>().name += " (copy)";

    m_selected = dst;
    logConsole(ICON_FA_CLONE " duplicated entity");
}

// ─────────────────────────────────────────────────────────────────────
//  Scene authoring -- mutate the Project + persist project.json
// ─────────────────────────────────────────────────────────────────────

void EditorLayer::newScene(std::string name)
{
    if (m_sceneManager.has(name))
    {
        logConsole("scene '" + name + "' already exists");
        return;
    }

    // Register in the manifest + persist project.json.
    m_project.addScene(name);
    if (auto r = m_project.save(); !r)
        std::println(std::cerr, "[Ω::EditorLayer] project save failed: {}", r.error().message);

    // Write an empty scene file up front so the scene persists immediately
    // (a throwaway empty World gives the serializer something to emit).
    {
        Engine::Scene::World empty { name };
        if (auto r = Engine::Scene::SceneSerializer::save(empty, m_project.scenePath(name)); !r)
            std::println(std::cerr, "[Ω::EditorLayer] new-scene file failed: {}", r.error().message);
    }

    // Create + wire the world, then switch to it (deferred).
    m_sceneManager.create(name).setOnEnter(
        [this](Engine::Scene::World& w) { setupWorld(w); });

    logConsole(ICON_FA_PLUS " new scene '" + name + "'");
    switchScene(name);
}

void EditorLayer::renameScene(std::string from, std::string to)
{
    if (m_sceneManager.has(to))
    {
        logConsole("scene '" + to + "' already exists");
        return;
    }

    bool const wasActive =
        m_sceneManager.active() && m_sceneManager.active()->name() == from;

    // Persist current edits to the OLD file first, so the rename carries
    // them (the new world reloads from the moved file).
    if (wasActive)
        saveScene();

    // Move the scene file on disk.
    std::error_code ec;
    std::filesystem::rename(m_project.scenePath(from), m_project.scenePath(to), ec);
    if (ec)
    {
        std::println(std::cerr, "[Ω::EditorLayer] rename file failed: {}", ec.message());
        return;
    }

    // Update + persist the manifest.
    m_project.renameScene(from, to);
    if (auto r = m_project.save(); !r)
        std::println(std::cerr, "[Ω::EditorLayer] project save failed: {}", r.error().message);

    // Swap the world. The new one loads from the moved file via setupWorld.
    m_sceneManager.create(to).setOnEnter(
        [this](Engine::Scene::World& w) { setupWorld(w); });

    if (wasActive)
    {
        // Switch to the new world; remove the old one only AFTER the
        // deferred switch applies (handled in onUpdate).
        m_selected = {};
        m_sceneManager.switchTo(to);
        m_pendingSceneRemoval = from;
    }
    else
    {
        m_sceneManager.remove(from);
    }

    logConsole(ICON_FA_PEN " renamed '" + from + "' -> '" + to + "'");
}

void EditorLayer::deleteScene(std::string name)
{
    if (m_project.scenes().size() <= 1)
    {
        logConsole("can't delete the last scene");
        return;
    }

    bool const isActive =
        m_sceneManager.active() && m_sceneManager.active()->name() == name;

    // Drop from the manifest + disk.
    m_project.removeScene(name);
    if (auto r = m_project.save(); !r)
        std::println(std::cerr, "[Ω::EditorLayer] project save failed: {}", r.error().message);

    std::error_code ec;
    std::filesystem::remove(m_project.scenePath(name), ec);

    if (isActive)
    {
        // Switch away first; defer the world removal until that applies so
        // we never free the world we're still rendering this frame.
        m_selected = {};
        m_sceneManager.switchTo(m_project.scenes().front());
        m_pendingSceneRemoval = name;
    }
    else
    {
        m_sceneManager.remove(name);
    }

    logConsole(ICON_FA_TRASH " deleted scene '" + name + "'");
}

void EditorLayer::setStartupScene(std::string name)
{
    m_project.setStartupScene(std::move(name));
    if (auto r = m_project.save(); !r)
        std::println(std::cerr, "[Ω::EditorLayer] project save failed: {}", r.error().message);
    logConsole(ICON_FA_STAR " startup scene -> " + m_project.startupScene());
}