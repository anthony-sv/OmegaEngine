module;

#include "imgui.h"
#include "glm/glm.hpp"
#ifdef _WIN32
#   include "TitlebarState.hpp"
#endif

module EditorLayer;

import Engine.Core;
import Engine.Renderer;

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
}

void EditorLayer::onDetach()
{
    m_framebuffer.reset();
    m_camera.reset();
    m_texture.reset();

    // Shutdown the batch renderer (frees its internal GPU resources).
    Engine::Renderer::Renderer2D::shutdown();

    std::println("[Ω::EditorLayer] detached");
}

void EditorLayer::onUpdate(float dt)
{
    if (!m_camera || !m_viewportHovered) return;

    // ── Temporary camera controls ───────────────────────────────
    // WASD = pan, Q/E = rotate, mouse wheel = zoom.
    // Uses ImGui's input queries so we don't need a separate input
    // system yet. These will be replaced by a proper CameraController
    // once the editor input pipeline is in place.

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

    if (ImGui::IsKeyDown(ImGuiKey_W))  { position.x -= s * step; position.y += c * step; }
    if (ImGui::IsKeyDown(ImGuiKey_S))  { position.x += s * step; position.y -= c * step; }
    if (ImGui::IsKeyDown(ImGuiKey_A))  { position.x -= c * step; position.y -= s * step; }
    if (ImGui::IsKeyDown(ImGuiKey_D))  { position.x += c * step; position.y += s * step; }

    if (ImGui::IsKeyDown(ImGuiKey_Q))  rotation += rotateSpeed * dt;
    if (ImGui::IsKeyDown(ImGuiKey_E))  rotation -= rotateSpeed * dt;

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

    // ── End the batch ───────────────────────────────────────────
    // This is where the actual GPU work happens:
    //   1. Upload the staged vertices to the dynamic VBO.
    //   2. Bind all textures that were used this batch.
    //   3. Issue ONE glDrawElements call for all 5 quads above.
    Engine::Renderer::Renderer2D::endBatch();

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

    if(m_showInspector)
    {
        ImGui::Begin("Inspector", &m_showInspector);
        ImGui::TextDisabled("No entity selected");
        ImGui::End();
    }

    if(m_showHierarchy)
    {
        ImGui::Begin("Scene Hierarchy", &m_showHierarchy);
        ImGui::Text("(empty scene)");
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