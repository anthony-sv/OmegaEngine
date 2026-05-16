export module ImGuiLayer;

import Engine.Core;

export class ImGuiLayer final: public Engine::Core::ILayer {
public:
    ImGuiLayer();

    void onAttach() override;

    void onDetach() override;

    void onRender(float) override;

    void onImGuiRender() override;
}; // class ImGuiLayer