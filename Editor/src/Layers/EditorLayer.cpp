module;

#include "imgui.h"
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

    // Ω::Phase 4 — buffer abstractions ────────────────────────────
    // Same colored quad as Phase 3, but raw OpenGL calls are now
    // wrapped in VertexBuffer, IndexBuffer, VertexArray, and
    // RenderCommand. No GL headers needed in this file anymore.

    // ── Shader (from external GLSL files) ──────────────────────
    auto shaderResult = Engine::Renderer::Shader::fromFiles(
        "assets/shaders/test_quad/vertex.glsl",
        "assets/shaders/test_quad/fragment.glsl"
    );
    if (!shaderResult)
    {
        std::println(std::cerr, "[Ω::EditorLayer] shader failed: {}", shaderResult.error().message);
        return;
    }
    m_testShader.emplace(std::move(*shaderResult));

    // ── Vertex + index data ─────────────────────────────────────
    constexpr float vertices[] =
    {
        //  x      y       r     g     b       u     v
        -0.5f, -0.5f,   1.0f, 1.0f, 1.0f,   0.0f, 0.0f,   // 0 — bottom-left
         0.5f, -0.5f,   1.0f, 1.0f, 1.0f,   1.0f, 0.0f,   // 1 — bottom-right
         0.5f,  0.5f,   1.0f, 1.0f, 1.0f,   1.0f, 1.0f,   // 2 — top-right
        -0.5f,  0.5f,   1.0f, 1.0f, 1.0f,   0.0f, 1.0f,   // 3 — top-left
    };

    constexpr std::uint32_t indices[] = 
    { 
        0, 1, 2, 
        0, 2, 3 
    };

    // ── GPU buffers ─────────────────────────────────────────────
    // The raw DSA calls from Phase 3 (glCreateBuffers, glNamedBuffer*,
    // glVertexArray*) are now encapsulated in these RAII wrappers.
    // The VertexArray reads the BufferLayout to auto-configure
    // attribute format, stride, and binding — no manual index math.
    using Engine::Renderer::VertexBuffer;
    using Engine::Renderer::IndexBuffer;
    using Engine::Renderer::VertexArray;
    using Engine::Renderer::ShaderDataType;

    auto vb = VertexBuffer::create(vertices, sizeof(vertices));
    vb.setLayout({
        { ShaderDataType::Float2, "a_Position" },
        { ShaderDataType::Float3, "a_Color" },
        { ShaderDataType::Float2, "a_TexCoord" },
    });

    auto ib = IndexBuffer::create(indices);

    auto va = VertexArray::create();
    va.addVertexBuffer(vb);
    va.setIndexBuffer(ib);

    m_vertexBuffer.emplace(std::move(vb));
    m_indexBuffer.emplace(std::move(ib));
    m_vertexArray.emplace(std::move(va));

    std::println("[Ω::EditorLayer] quad ready");

    // ── Test texture ───────────────────────────────────────────
    // Load a real image to verify the texture pipeline end-to-end.
    // The fragment shader multiplies: texture() * vertex color,
    // so vertex colors tint the image. Set all colors to white
    // (1,1,1) if you want to see the texture unmodified.
    auto texResult = Engine::Renderer::Texture2D::create("assets/textures/wall.jpg");
    if (!texResult)
    {
        std::println(std::cerr, "[Ω::EditorLayer] texture failed: {}", texResult.error().message);
        return;
    }
    m_texture.emplace(std::move(*texResult));

    // Tell the shader which texture unit to sample from.
    // This only needs to happen once — slot 0 won't change.
    m_testShader->bind();
    m_testShader->setInt("u_Texture", 0);
    m_testShader->unbind();

    // ── Framebuffer (FBO) ───────────────────────────────────────
    // Instead of rendering to the window (default framebuffer), we
    // render into an off-screen texture that ImGui displays in the
    // Viewport panel. Initial size is arbitrary — it resizes to
    // match the panel dimensions on the first frame.
    auto fbResult = Engine::Renderer::Framebuffer::create(1280, 720);
    if (fbResult)
        m_framebuffer.emplace(std::move(*fbResult));
    else
        std::println(std::cerr, "[Ω::EditorLayer] framebuffer failed: {}", fbResult.error().message);
}

void EditorLayer::onDetach()
{
    m_framebuffer.reset();
    m_texture.reset();
    m_testShader.reset();
    m_vertexArray.reset();
    m_indexBuffer.reset();
    m_vertexBuffer.reset();
    std::println("[Ω::EditorLayer] detached");
}

void EditorLayer::onRender(float /*alpha*/)
{
    if (!m_testShader || !m_framebuffer || !m_vertexArray || !m_texture) return;

    // Bind the FBO — all subsequent draw calls render into its color texture instead of the window.
    m_framebuffer->bind();
    Engine::Renderer::RenderCommand::setClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    Engine::Renderer::RenderCommand::clear();

    m_testShader->bind();
    m_texture->bind(0);
    m_vertexArray->bind();
    Engine::Renderer::RenderCommand::drawIndexed(m_vertexArray->indexCount());
    m_vertexArray->unbind();
    m_testShader->unbind();

    // unbind() returns to the default framebuffer and restores the
    // viewport dimensions that were active before bind().
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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport", &m_showViewport);
        ImGui::PopStyleVar();

        if (m_framebuffer)
        {
            auto const size = ImGui::GetContentRegionAvail();

            if (size.x > 0 && size.y > 0)
            {
                auto const w = static_cast<std::uint32_t>(size.x);
                auto const h = static_cast<std::uint32_t>(size.y);

                // Resize the FBO when the panel dimensions change (user
                // dragging a splitter, maximizing, etc.). The texture is
                // recreated at the new resolution.
                if (w != m_framebuffer->width() || h != m_framebuffer->height())
                    m_framebuffer->resize(w, h);

                // Display the FBO's color texture. UV flip: (0,1)→(1,0)
                // because OpenGL textures have origin at bottom-left but
                // ImGui expects origin at top-left.
                auto const texId = static_cast<ImTextureID>(m_framebuffer->colorAttachment());
                ImGui::Image(texId, size, ImVec2(0, 1), ImVec2(1, 0));
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