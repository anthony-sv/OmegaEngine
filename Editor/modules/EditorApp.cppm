export module EditorApp;

import Engine.Core;
import Engine.Scene;   // Scene::Project

// =================================================================
//
//  EditorApp -- the generic editor TOOL: opens a project and edits it.
//
// =================================================================
//
// Like the Runtime, the editor carries no game content. It is handed a
// Project (the unit of content) and edits its scenes. The WINDOW here is
// EDITOR chrome (borderless, dark, with the custom titlebar) -- NOT the
// project's game-window config (that belongs to the Runtime). The
// project's NAME is folded into the title so you can see what's open.
//
// =================================================================

export class EditorApp final: public Engine::Core::Application {
public:
    explicit EditorApp(Engine::Scene::Project project);

protected:
    void onInit()     override;
    void onShutdown() override;

private:
    Engine::Scene::Project m_project;   // the content being edited
}; // class EditorApp