export module SandboxApp;

import Engine.Core;

// =================================================================
//
//  SandboxApp -- a minimal game-style consumer of the engine.
//
// =================================================================
//
// Unlike EditorApp (which renders into an off-screen framebuffer
// and presents it through an ImGui docking UI), SandboxApp is a
// "bare game window": it renders the scene straight to the window's
// default framebuffer, no editor chrome. This exercises the engine
// purely through its PUBLIC API -- the same path a real game built
// on OmegaEngine would take. If something here is awkward, it's a
// signal the public API needs work.
//
// =================================================================

export class SandboxApp final : public Engine::Core::Application
{
public:
    SandboxApp();

protected:
    void onInit()     override;
    void onShutdown() override;
}; // class SandboxApp