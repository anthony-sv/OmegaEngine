export module EditorApp;

import Engine.Core;

export class EditorApp final: public Engine::Core::Application {
public:
    EditorApp();

protected:
    void onInit() override;

    void onShutdown() override;
}; // class EditorApp