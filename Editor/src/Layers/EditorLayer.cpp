module;

#include "imgui.h"
#include "glm/glm.hpp"
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
    }
}

EditorLayer::EditorLayer()
    : ILayer { "Ω::EditorLayer" }
{}

void EditorLayer::onAttach()
{
    std::println("[Ω::EditorLayer] attached");

    // ── Batch renderer ─────────────────────────────────────────
    // Renderer2D::init() creates all the internal GPU resources
    // the batch renderer needs: its own shader, a 1x1 white pixel
    // texture, a dynamic VBO, a static IBO, and a VAO. None of
    // these are exposed — the batch renderer is self-contained.
    auto initResult = Engine::Renderer::Renderer2D::init();
    if (!initResult)
    {
        std::println(std::cerr, "[Ω::EditorLayer] Renderer2D init failed: {}", initResult.error().message);
        return;
    }

    // ── Test texture ───────────────────────────────────────────
    // Load an image to demonstrate textured quads in the batch.
    // The batch renderer doesn't own this — we pass it by
    // reference in drawQuad(), and it binds the GL texture at
    // flush time. The Texture2D must outlive the frame.
    auto texResult = Engine::Renderer::Texture2D::create("assets/textures/wall.jpg");
    if (!texResult)
    {
        std::println(std::cerr, "[Ω::EditorLayer] texture failed: {}", texResult.error().message);
        return;
    }
    m_texture.emplace(std::move(*texResult));

    // ── Sprite sheet (P8) ─────────────────────────────────────
    // Load a sprite sheet atlas. SubTexture2D instances will
    // reference sub-regions of this texture via UV coordinates.
    // The Texture2D must outlive all SubTexture2D that point to it.
    auto sheetResult = Engine::Renderer::Texture2D::create("assets/textures/Sheet.png");
    if (sheetResult)
    {
        m_spriteSheet.emplace(std::move(*sheetResult));

        // Extract individual sprites from the grid-based sheet.
        // createFromGrid converts (column, row) image-space
        // coordinates into OpenGL UV rectangles, handling the
        // Y-axis flip automatically.
        //
        // Sheet.png is 128x64, laid out as a 2-column × 1-row grid.
        // Each cell is 64x64 pixels:
        //   column 0 = red-dress character
        //   column 1 = black-dress character

        m_spriteA.emplace(Engine::Renderer::SubTexture2D::createFromGrid(
            *m_spriteSheet,
            { 0.0f, 0.0f },        // column 0, row 0 (red dress)
            { 64.0f, 64.0f }       // each cell is 64x64 pixels
        ));

        m_spriteB.emplace(Engine::Renderer::SubTexture2D::createFromGrid(
            *m_spriteSheet,
            { 1.0f, 0.0f },        // column 1, row 0 (black dress)
            { 64.0f, 64.0f }
        ));

        m_spriteC.emplace(Engine::Renderer::SubTexture2D::createFromGrid(
            *m_spriteSheet,
            { 1.0f, 0.0f },        // column 1 again, rotated in the scene
            { 64.0f, 64.0f }
        ));

        std::println(
            "[Ω::EditorLayer] sprite sheet loaded ({}x{}) — 3 sub-textures created",
            m_spriteSheet->width(), 
            m_spriteSheet->height()
        );
    }
    else
    {
        std::println(std::cerr, "[Ω::EditorLayer] sprite sheet failed: {}", sheetResult.error().message);
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

    // ── ECS world ──────────────────────────────────────
    // The viewport now renders a managed World not a loose Registry. 
    // 
    // The world's content is built in its onEnter hook: three quads carrying angular Velocity2D, plus a
    // MovementSystem that spins them -- so the editor viewport visibly
    // exercises the system model too.
    //
    // create(name) auto-adds a NameComponent, so these entities are
    // already identifiable (useful once the Hierarchy panel reads the
    // active world's registry).
    auto& world = m_sceneManager.create("Editor");
    world.setOnEnter([this](Engine::Scene::World& w)
    {
        auto spin = [](float deg) {
            return Engine::ECS::Velocity2D{ .linear = { 0.0f, 0.0f }, .angular = deg };
        };

        auto e1 = w.createEntity("ECS Quad — magenta");
        e1.add<Engine::ECS::Transform>(Engine::ECS::Transform{
            .position = { 2.0f, 0.6f }, .rotation = 0.0f, .scale = { 0.4f, 0.4f } });
        e1.add<Engine::ECS::SpriteRenderer>(Engine::ECS::SpriteRenderer{
            .color = { 0.9f, 0.2f, 0.9f, 1.0f } });
        e1.add<Engine::ECS::Velocity2D>(spin(60.0f));

        auto e2 = w.createEntity("ECS Quad — cyan");
        e2.add<Engine::ECS::Transform>(Engine::ECS::Transform{
            .position = { 2.0f, -0.1f }, .rotation = 0.0f, .scale = { 0.4f, 0.4f } });
        e2.add<Engine::ECS::SpriteRenderer>(Engine::ECS::SpriteRenderer{
            .color = { 0.2f, 0.9f, 0.9f, 1.0f } });
        e2.add<Engine::ECS::Velocity2D>(spin(-60.0f));

        auto e3 = w.createEntity("ECS Quad — orange");
        e3.add<Engine::ECS::Transform>(Engine::ECS::Transform{
            .position = { 2.0f, -0.8f }, .rotation = 25.0f, .scale = { 0.4f, 0.4f } });
        e3.add<Engine::ECS::SpriteRenderer>(Engine::ECS::SpriteRenderer{
            .color = { 1.0f, 0.6f, 0.1f, 1.0f } });
        e3.add<Engine::ECS::Velocity2D>(spin(90.0f));

        w.addSystem<Engine::Systems::MovementSystem>();

        // ── Animated sprite ─────────────────────────────────────────
        // Reuse the already-loaded sprite sheet + its two cell sub-
        // textures (m_spriteA/B) as a 2-frame animation. The
        // AnimationSystem advances SpriteAnimation -> writes the
        // SpriteRenderer UVs; the RenderSystem draws it in the viewport.
        if (m_spriteSheet && m_spriteA && m_spriteB)
        {
            auto hero = w.createEntity("Animated Hero");
            hero.add<Engine::ECS::Transform>(Engine::ECS::Transform{
                .position = { 0.0f, 1.2f }, .rotation = 0.0f, .scale = { 0.7f, 0.7f } });
            hero.add<Engine::ECS::SpriteRenderer>(Engine::ECS::SpriteRenderer{
                .texture = &*m_spriteSheet,
                .uvMin   = m_spriteA->uvMin(),
                .uvMax   = m_spriteA->uvMax() });
            hero.add<Engine::ECS::SpriteAnimation>(Engine::ECS::SpriteAnimation{
                .frames = {
                    { .uvMin = m_spriteA->uvMin(), .uvMax = m_spriteA->uvMax() },
                    { .uvMin = m_spriteB->uvMin(), .uvMax = m_spriteB->uvMax() },
                },
                .frameDuration = 0.5f,
                .looping       = true,
                .playing       = true });

            w.addSystem<Engine::Systems::AnimationSystem>();
        }

        std::println("[Ω::EditorLayer] world '{}' built — {} entities", w.name(), w.registry().entityCount());
    });

    m_sceneManager.switchTo("Editor");
}

void EditorLayer::onDetach()
{
    m_framebuffer.reset();
    m_camera.reset();

    // SubTexture2D must be destroyed BEFORE the atlas they reference.
    // SubTexture2D holds a non-owning pointer to the Texture2D, so
    // the atlas must still be alive when SubTexture2D's destructor runs.
    // (In practice SubTexture2D is trivially destructible, but keeping
    // the order correct is good hygiene for future changes.)
    m_spriteA.reset();
    m_spriteB.reset();
    m_spriteC.reset();
    m_spriteSheet.reset();

    m_texture.reset();

    // Shutdown the batch renderer (frees its internal GPU resources).
    Engine::Renderer::Renderer2D::shutdown();

    std::println("[Ω::EditorLayer] detached");
}

void EditorLayer::onUpdate(float dt)
{
    // Advance the active world's systems (spins the ECS quads). Runs
    // every frame, regardless of whether the viewport has focus.
    m_sceneManager.onUpdate(dt);

    if (!m_camera || !m_viewportHovered) return;

    // ── Camera controls ─────────────────────────────────────────
    // WASD = pan, Q/E = rotate, mouse wheel = zoom. Now driven by the
    // engine's Input service (GLFW under the hood) instead of borrowing
    // ImGui's key queries. Gated on m_viewportHovered above so typing in
    // other panels doesn't move the camera.

    using Engine::Core::Key;
    using Input = Engine::Core::Input;

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

void EditorLayer::onRender(float /*alpha*/)
{
    if (!m_framebuffer || !m_camera) return;

    // Reset per-frame stats so the Console panel shows this frame's numbers.
    Engine::Renderer::Renderer2D::resetStats();

    // Bind the FBO — all draw calls go into its color texture.
    m_framebuffer->bind();
    Engine::Renderer::RenderCommand::setClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    Engine::Renderer::RenderCommand::clear();

    // ── Begin the batch ─────────────────────────────────────────
    // After this call, every drawQuad() writes into a CPU staging
    // buffer. Nothing goes to the GPU until endBatch() flushes.
    Engine::Renderer::Renderer2D::beginBatch(*m_camera);

    // ── Textured quad (wall) ────────────────────────────────────
    // Same wall.jpg from Phase 5, now drawn through the batch
    // renderer instead of a manual VAO/shader setup.
    if (m_texture)
    {
        Engine::Renderer::Renderer2D::drawQuad(
            { -0.5f, -0.5f },      // position (bottom-left corner)
            {  1.0f,  1.0f },      // size (1x1 world unit)
            *m_texture             // texture (tint = white, tiling = 1.0)
        );
    }

    // ── Colored quads ───────────────────────────────────────────
    // These use the internal 1x1 white texture: white * color = color.
    // All of these quads are batched into the same draw call as the
    // textured quad above — that's the whole point of batching.
    Engine::Renderer::Renderer2D::drawQuad(
        {  0.8f, -0.3f },
        {  0.4f,  0.4f },
        { 1.0f, 0.2f, 0.3f, 1.0f }    // red
    );

    Engine::Renderer::Renderer2D::drawQuad(
        { -1.4f,  0.3f },
        {  0.6f,  0.6f },
        { 0.2f, 0.3f, 1.0f, 1.0f }    // blue
    );

    Engine::Renderer::Renderer2D::drawQuad(
        { -0.8f, -1.2f },
        {  0.5f,  0.5f },
        { 0.1f, 0.8f, 0.4f, 1.0f }    // green
    );

    // ── Rotated quad ────────────────────────────────────────────
    // Rotation is in degrees, counter-clockwise, around the center.
    // The batch renderer computes the rotated corners on the CPU
    // and writes them as pre-transformed vertices into the batch.
    Engine::Renderer::Renderer2D::drawRotatedQuad(
        { 0.3f, 0.8f },
        { 0.5f, 0.5f },
        45.0f,                          // 45 degrees CCW
        { 1.0f, 0.8f, 0.1f, 1.0f }     // yellow
    );

    // ── Sprite sheet quads (P8) ─────────────────────────────────
    // These quads use SubTexture2D — each one renders a different
    // sub-region of the same sprite sheet atlas. The atlas texture
    // only occupies ONE texture slot in the batch, no matter how
    // many different sub-textures we draw from it. That's the
    // performance win: 100 different sprites, 1 texture slot.
    if (m_spriteA)
    {
        Engine::Renderer::Renderer2D::drawQuad(
            { -2.0f, -0.5f },      // to the left of the wall quad
            {  0.5f,  0.5f },
            *m_spriteA
        );
    }

    if (m_spriteB)
    {
        Engine::Renderer::Renderer2D::drawQuad(
            { -2.0f,  0.2f },      // above sprite A
            {  0.5f,  0.5f },
            *m_spriteB
        );
    }

    if (m_spriteC)
    {
        // Draw spriteC rotated to show that drawRotatedQuad works
        // with sub-textures too. Same rotation math as any other
        // rotated quad — the only difference is the UV sub-region.
        Engine::Renderer::Renderer2D::drawRotatedQuad(
            { -2.0f,  0.9f },      // above sprite B
            {  0.5f,  0.5f },
            30.0f,                  // slight tilt
            *m_spriteC
        );
    }

    // ── End the manual demo batch ───────────────────────────────
    // Flushes the hard-coded quads above (wall, colored, sprites) in
    // a single draw call.
    Engine::Renderer::Renderer2D::endBatch();

    // ── ECS render pass ─────────────────────────────────────────
    // Hand the ACTIVE world's registry to the RenderSystem. It opens
    // its own batch, iterates every entity with Transform +
    // SpriteRenderer, draws each, and flushes. The EditorLayer no
    // longer owns a Registry — it asks the SceneManager for the active
    // world. The FBO is still bound here, so these quads land in the
    // same off-screen target as the demo batch.
    if (auto* world = m_sceneManager.active())
        Engine::Systems::RenderSystem::render(world->registry(), *m_camera);

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
            ImGui::EndMenu();
        }

        ImVec2 const tl = ImGui::GetWindowPos();
        ImVec2 const br = { tl.x + ImGui::GetWindowWidth(), tl.y + g_titlebarHeight };
        auto* dl = ImGui::GetWindowDrawList();

        char const* title = "ΩmegaEngine Editor";
        ImVec2 const textSize = ImGui::CalcTextSize(title);
        float  const textX = tl.x + (br.x - tl.x - textSize.x) * 0.5f;
        float  const textY = tl.y + (g_titlebarHeight - textSize.y) * 0.5f;
        dl->AddText({ textX, textY }, IM_COL32(180, 178, 200, 255), title);

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
        if(ImGui::Button(" - ##ΩMin", { btnW, frameH }))
            win.minimize();

        ImGui::SameLine(0, 0);

        if(ImGui::Button(win.isMaximized() ? " = ##ΩMax" : " [] ##ΩMax", { btnW, frameH }))
        {
            if(win.isMaximized())  win.restore();
            else                   win.maximize();
        }

        ImGui::SameLine(0, 0);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.86f, 0.2f, 0.2f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.70f, 0.1f, 0.1f, 1.0f });
        if(ImGui::Button(" x ##ΩClose", { btnW, frameH }))
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
        ImGui::TextColored({ 0.3f, 0.9f, 0.5f, 1.0f },
                           "[Ω] OmegaEngine started successfully");
        ImGui::TextColored({ 0.5f, 0.5f, 0.7f, 1.0f },
                           "[Ω] Docking + Viewports enabled");
        ImGui::TextColored({ 0.5f, 0.5f, 0.7f, 1.0f },
                           "[Ω] Fixed timestep: 60Hz (%.2fms)", 1000.0f / 60.0f);

        // ── Batch renderer stats ────────────────────────────────
        // Shows how many draw calls and quads were submitted this
        // frame. With batching, 5 quads = 1 draw call (not 5).
        auto const stats = Engine::Renderer::Renderer2D::stats();
        ImGui::Separator();
        ImGui::TextColored({ 0.5f, 0.7f, 0.9f, 1.0f },
                           "[Ω] Draw calls: %u  |  Quads: %u",
                           stats.drawCalls, stats.quadCount);

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