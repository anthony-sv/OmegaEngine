module SandboxApp;

import Engine.Core;
import SandboxLayer;
import std;

// A decorated window (unlike the Editor's borderless custom titlebar).
// vsync on so the demo doesn't spin the GPU at thousands of FPS.
SandboxApp::SandboxApp()
    : Engine::Core::Application {
        Engine::Core::WindowProps {
            .title     = "ΩmegaEngine Sandbox",
            .width     = 1280,
            .height    = 720,
            .vsync     = true,
            .decorated = true
        }
    }
{}

void SandboxApp::onInit()
{
    // One gameplay layer. No ImGui overlay -- this is a bare game window.
    pushLayer<SandboxLayer>();
    std::println("[Ω::SandboxApp] initialised — Ω ready");
}

void SandboxApp::onShutdown()
{
    std::println("[Ω::SandboxApp] shutting down — Ω offline");
}