module;

#include "glad/glad.h"
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

    // Ω::Phase 1 — test triangle ─────────────────────────────────
    // Renders to the default framebuffer (behind ImGui).

    // ── Shader sources (GLSL) ───────────────────────────────────
    // Vertex shader: runs once per vertex — positions geometry.
    // Fragment shader: runs once per pixel — determines color.
    // The GPU interpolates 'out' variables across the triangle surface,
    // so per-vertex colors blend smoothly ("varying interpolation").
    // layout(location = N) must match the attribute index in the VAO.
    constexpr auto vertSrc = R"glsl(
#version 460 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec3 a_Color;

out vec3 v_Color;

void main() {
    gl_Position = vec4(a_Position, 0.0, 1.0);
    v_Color = a_Color;
}
)glsl";

    constexpr auto fragSrc = R"glsl(
#version 460 core
in vec3 v_Color;

out vec4 o_Color;

void main() {
    o_Color = vec4(v_Color, 1.0);
}
)glsl";

    auto shaderResult = Engine::Renderer::Shader::fromSources(vertSrc, fragSrc);
    if (!shaderResult) 
    {
        std::println(std::cerr, "[Ω::EditorLayer] test shader failed: {}",
            shaderResult.error().message);
        return;
    }
    m_testShader.emplace(std::move(*shaderResult));

    // ── Vertex data (NDC) ───────────────────────────────────────
    // Normalized Device Coordinates: visible range is [-1, 1] on both
    // axes. gl_Position output lands here after the vertex shader.
    // Per-vertex layout: position (vec2) + color (vec3) = 5 floats.
    constexpr float vertices[] = 
    {
        //  x      y       r     g     b
        -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   // bottom-left  — red
         0.5f, -0.5f,   0.0f, 1.0f, 0.0f,   // bottom-right — green
         0.0f,  0.5f,   0.0f, 0.0f, 1.0f,   // top-center   — blue
    };

    // ── VAO (Vertex Array Object) ───────────────────────────────
    // Captures the vertex format — how many attributes, their types,
    // offsets, and which buffer to read from. Binding a VAO restores
    // all of this state in a single call.
    glCreateVertexArrays(1, &m_testVAO);

    // ── VBO (Vertex Buffer Object) ──────────────────────────────
    // GPU-side memory holding the raw vertex data. DSA functions
    // (glCreate*, glNamed*) operate by object ID — no global bind.
    // glNamedBufferStorage: immutable alloc, ideal for static geometry.
    glCreateBuffers(1, &m_testVBO);
    glNamedBufferStorage(m_testVBO, sizeof(vertices), vertices, 0);

    // ── Vertex attributes ───────────────────────────────────────
    // Tell the VAO how to interpret each vertex in the VBO:
    //
    // VertexArrayVertexBuffer — bind VBO to VAO at binding index 0.
    //   stride = 20 bytes (5 floats) between consecutive vertices.
    //
    // VertexArrayAttribFormat — describe one attribute in the vertex.
    //   attr 0 (a_Position): 2 floats, offset 0 bytes.
    //   attr 1 (a_Color):    3 floats, offset 8 bytes (past vec2).
    //
    // VertexArrayAttribBinding — wire attribute N to binding index 0.
    //   This indirection lets multiple attributes share one buffer.
    constexpr auto stride = static_cast<GLsizei>(5 * sizeof(float));

    glVertexArrayVertexBuffer(m_testVAO, 0, m_testVBO, 0, stride);

    glEnableVertexArrayAttrib(m_testVAO, 0);
    glVertexArrayAttribFormat(m_testVAO, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_testVAO, 0, 0);

    glEnableVertexArrayAttrib(m_testVAO, 1);
    glVertexArrayAttribFormat(m_testVAO, 1, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
    glVertexArrayAttribBinding(m_testVAO, 1, 0);

    std::println("[Ω::EditorLayer] test triangle ready (VAO={}, VBO={})", m_testVAO, m_testVBO);

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
    m_testShader.reset();
    if (m_testVBO) glDeleteBuffers(1, &m_testVBO);
    if (m_testVAO) glDeleteVertexArrays(1, &m_testVAO);
    std::println("[Ω::EditorLayer] detached");
}

void EditorLayer::onRender(float /*alpha*/) 
{
    if (!m_testShader || !m_framebuffer) return;

    // Bind the FBO — all subsequent draw calls render into its color
    // texture instead of the window. bind() also sets glViewport to
    // match the FBO dimensions (critical: mismatched viewport = wrong scale).
    m_framebuffer->bind();
    glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_testShader->bind();
    glBindVertexArray(m_testVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
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