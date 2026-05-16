module;

#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#ifdef _WIN32
#   include <Windows.h>
#   include <windowsx.h>
#   include <dwmapi.h>
#   pragma comment(lib, "dwmapi.lib")
#   include "TitlebarState.hpp"
extern "C" HWND glfwGetWin32Window(GLFWwindow*);
#endif

module ImGuiLayer;

import Engine.Core;

// Ω::Win32 borderless window proc ────────────────────────────────────────
#ifdef _WIN32
static WNDPROC s_prevWndProc = nullptr;
static HWND    s_cachedHwnd  = nullptr;

static LRESULT CALLBACK EditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_NCCALCSIZE:
        {
            if(wParam == TRUE && IsZoomed(hwnd)) {
                auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi {};
                mi.cbSize = sizeof(mi);
                if(GetMonitorInfo(mon, &mi))
                    p->rgrc[0] = mi.rcWork;
            }
            return 0;
        }

        case WM_NCHITTEST:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);

            RECT rc;
            GetClientRect(hwnd, &rc);

            if(!IsZoomed(hwnd)) {
                int bx = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                int by = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

                bool l = pt.x < bx;
                bool r = pt.x >= rc.right - bx;
                bool t = pt.y < by;
                bool b = pt.y >= rc.bottom - by;

                if(t && l) return HTTOPLEFT;
                if(t && r) return HTTOPRIGHT;
                if(b && l) return HTBOTTOMLEFT;
                if(b && r) return HTBOTTOMRIGHT;
                if(l)      return HTLEFT;
                if(r)      return HTRIGHT;
                if(t)      return HTTOP;
                if(b)      return HTBOTTOM;
            }

            if(pt.y < g_titlebarHeight && !g_imguiWantsInput)
                return HTCAPTION;

            return HTCLIENT;
        }

        case WM_GETMINMAXINFO:
        {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = 400;
            mmi->ptMinTrackSize.y = 300;
            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi {};
            mi.cbSize = sizeof(mi);
            if(GetMonitorInfo(mon, &mi)) {
                mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
                mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
                mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
            }
            return 0;
        }

        case WM_NCACTIVATE:
            return TRUE;

        case WM_ERASEBKGND:
            return 1;
    }

    return CallWindowProcW(s_prevWndProc, hwnd, msg, wParam, lParam);
}
#endif

ImGuiLayer::ImGuiLayer(): 
    ILayer { "Ω::ImGuiLayer" } {}

void ImGuiLayer::onAttach() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename = "omega_editor_layout.ini";
    
    ImGui::StyleColorsDark();
    
    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesGreek());
    builder.BuildRanges(&ranges);
    
    io.Fonts->AddFontFromFileTTF(
        "assets/fonts/JetBrainsMonoNL-Regular.ttf",
        15.0f,
        nullptr,
        ranges.Data);
    
    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        auto& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    
    auto* nativeWindow = Engine::Core::Application::get().window().nativeHandle();
    ImGui_ImplGlfw_InitForOpenGL(nativeWindow, true);
    ImGui_ImplOpenGL3_Init("#version 460");

#ifdef _WIN32
    s_cachedHwnd = glfwGetWin32Window(nativeWindow);
    s_prevWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(s_cachedHwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditorWndProc))
        );
    SetWindowPos(s_cachedHwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#endif
    
    std::println("[Ω::ImGuiLayer] attached — docking + viewports");
}

void ImGuiLayer::onDetach() {
#ifdef _WIN32
    if(s_prevWndProc && s_cachedHwnd) {
        SetWindowLongPtrW(s_cachedHwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_prevWndProc));
        s_prevWndProc = nullptr;
        s_cachedHwnd  = nullptr;
    }
#endif
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    std::println("[Ω::ImGuiLayer] detached");
}

void ImGuiLayer::onRender(float) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::onImGuiRender() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    if(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        auto* backup = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup);
    }
}