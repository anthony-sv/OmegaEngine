module;

#include "imgui.h"
#include "imgui-node-editor/imgui_node_editor.h"

module NodeEditor;

import NodeGraph;
import NodeCodegen;
import std;

namespace Editor::Nodes
{

    namespace ed = ax::NodeEditor;

    namespace
    {
        ImVec4 pinColor(PinType t)
        {
            using T = PinType;
            switch (t)
            {
                case T::Exec:  return { 0.90f, 0.90f, 0.90f, 1.0f };
                case T::Float: return { 0.40f, 0.85f, 0.45f, 1.0f };
                case T::Vec2:  return { 0.95f, 0.70f, 0.30f, 1.0f };
                case T::Bool:  return { 0.85f, 0.35f, 0.35f, 1.0f };
            }
            return { 1, 1, 1, 1 };
        }

        // Draw a pin connector: a triangle for exec wires, a circle for data wires
        // (filled when something is plugged in), then reserve its space.
        void drawPinIcon(PinType type, bool connected)
        {
            constexpr float r  = 7.0f;
            ImDrawList* const dl = ImGui::GetWindowDrawList();
            ImVec2 const      p  = ImGui::GetCursorScreenPos();
            ImVec2 const      c  { p.x + r, p.y + r };
            ImU32 const       col = ImGui::ColorConvertFloat4ToU32(pinColor(type));

            if (type == PinType::Exec)
            {
                ImVec2 const a { c.x - r * 0.5f, c.y - r };
                ImVec2 const b { c.x - r * 0.5f, c.y + r };
                ImVec2 const t { c.x + r * 0.7f, c.y };
                if (connected) dl->AddTriangleFilled(a, b, t, col);
                else           dl->AddTriangle(a, b, t, col, 1.8f);
            }
            else
            {
                if (connected) dl->AddCircleFilled(c, r * 0.72f, col);
                else           dl->AddCircle(c, r * 0.72f, col, 12, 1.8f);
            }
            ImGui::Dummy({ r * 2.0f, r * 2.0f });
        }

        // ── Live evaluator (calc graphs) ──
        // Compute a pure value-node's output, recursing through its inputs. Returns
        // nullopt when it can't be evaluated in the editor (a runtime value like dt,
        // or an unconnected/unknown node), so the panel shows it only when known.
        std::optional<double> evalOutput(Graph const& g, Node const& n,
                                         Pin const& out, int depth);

        std::optional<double> evalInput(Graph const& g, Pin const& in, int depth)
        {
            if (depth > 64)
                return std::nullopt;
            for (auto const& l : g.links)
                if (l.to == in.id)
                    for (auto const& m : g.nodes)
                        for (auto const& p : m.outputs)
                            if (p.id == l.from)
                                return evalOutput(g, m, p, depth + 1);
            return std::nullopt;
        }

        std::optional<double> evalOutput(Graph const& g, Node const& n,
                                         Pin const& out, int depth)
        {
            if (n.type == "Constant")
                return n.value;
            if (n.type == "Add" && n.inputs.size() >= 2)
            {
                auto const a = evalInput(g, n.inputs[0], depth);
                auto const b = evalInput(g, n.inputs[1], depth);
                return (a && b) ? std::optional { *a + *b } : std::nullopt;
            }
            if (n.type == "Multiply" && n.inputs.size() >= 2)
            {
                auto const a = evalInput(g, n.inputs[0], depth);
                auto const b = evalInput(g, n.inputs[1], depth);
                return (a && b) ? std::optional { *a * *b } : std::nullopt;
            }
            if (n.type == "Greater" && n.inputs.size() >= 2)
            {
                auto const a = evalInput(g, n.inputs[0], depth);
                auto const b = evalInput(g, n.inputs[1], depth);
                return (a && b) ? std::optional { (*a > *b) ? 1.0 : 0.0 } : std::nullopt;
            }
            return std::nullopt;   // dt / Time / KeyDown / actions -> runtime only
        }

        // Seed a fresh graph with the minimal useful skeleton.
        void seedDefault(Graph& g)
        {
            if (auto const* s = findSpec("OnUpdate")) addNode(g, *s,  40.0f, 80.0f);
            if (auto const* s = findSpec("Print"))    addNode(g, *s, 360.0f, 80.0f);
        }
    }

    NodeEditorPanel::~NodeEditorPanel()
    {
        // Intentionally NOT ed::DestroyEditor: this panel is a long-lived static
        // whose dtor runs at program exit, after ImGui is torn down -- destroying
        // then dereferences freed state. Leaking the context is harmless at exit.
    }

    void NodeEditorPanel::open(std::string const& name)
    {
        m_name   = name;
        m_seeded = true;
        if (!load(m_graph, "graphs/" + m_name + ".ngraph"))
        {
            m_graph = Graph {};
            seedDefault(m_graph);
        }
        m_fit = true;
    }

    void NodeEditorPanel::draw(bool* open)
    {
        // `###` keeps the window's dock identity stable while the visible
        // title tracks the graph being edited.
        if (!ImGui::Begin(("Node Graph \xE2\x80\x94 " + m_name + "###NodeGraph").c_str(), open))
        {
            ImGui::End();
            return;
        }

        if (m_ctx == nullptr)
        {
            ed::Config cfg;
            cfg.SettingsFile = nullptr;          // no on-disk layout (we serialize positions ourselves)
            m_ctx = ed::CreateEditor(&cfg);
        }
        auto* ctx = static_cast<ed::EditorContext*>(m_ctx);

        if (!m_seeded)
        {
            m_seeded = true;
            seedDefault(m_graph);
            m_fit = true;
        }

        // Pins that currently have a wire -> draw their icon filled.
        std::unordered_set<int> connected;
        for (auto const& l : m_graph.links)
        {
            connected.insert(l.from);
            connected.insert(l.to);
        }

        ImGui::TextDisabled("right-click: add node   |   drag pin -> pin: connect   |   middle-drag: pan   |   Del: remove");

        ImVec2 const openPopupPos = ImGui::GetMousePos();

        ed::SetCurrentEditor(ctx);
        ed::Begin("Canvas");

        // -- nodes --
        for (auto& node : m_graph.nodes)
        {
            auto const* spec = findSpec(node.type);

            ed::BeginNode(ed::NodeId(node.id));
            ImGui::TextUnformatted(node.title.c_str());
            ImGui::Dummy({ 0.0f, 2.0f });

            if (spec != nullptr && spec->hasValue)
            {
                float v = static_cast<float>(node.value);
                ImGui::SetNextItemWidth(90.0f);
                if (ImGui::DragFloat(("##v" + std::to_string(node.id)).c_str(), &v, 0.05f))
                    node.value = v;
            }

            // Choice param (e.g. Key Down's key). A combo would open a popup,
            // which imgui-node-editor can't host inside the canvas -- so step
            // through the options with a pair of arrow buttons instead.
            if (spec != nullptr && !spec->options.empty())
            {
                auto const& opts = spec->options;
                int idx = 0;
                for (int i = 0; i < static_cast<int>(opts.size()); ++i)
                    if (opts[i] == node.param)
                        idx = i;

                std::string const tag = std::to_string(node.id);
                int const count = static_cast<int>(opts.size());
                if (ImGui::ArrowButton(("##p" + tag).c_str(), ImGuiDir_Left))
                    node.param = opts[(idx + count - 1) % count];
                ImGui::SameLine();
                ImGui::TextUnformatted(node.param.c_str());
                ImGui::SameLine();
                if (ImGui::ArrowButton(("##n" + tag).c_str(), ImGuiDir_Right))
                    node.param = opts[(idx + 1) % count];
            }

            // Watch -- the calc-graph probe: show its input's live value big
            // and up front (the whole point of the node).
            if (node.type == "Watch" && !node.inputs.empty())
            {
                auto const v = evalInput(m_graph, node.inputs[0], 0);
                ImGui::TextUnformatted(v ? std::format("= {:g}", *v).c_str() : "= ?");
            }

            for (auto const& p : node.inputs)
            {
                ed::BeginPin(ed::PinId(p.id), ed::PinKind::Input);
                drawPinIcon(p.type, connected.contains(p.id));
                if (!p.name.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextUnformatted(p.name.c_str());
                }
                ed::EndPin();
            }
            for (auto const& p : node.outputs)
            {
                ed::BeginPin(ed::PinId(p.id), ed::PinKind::Output);

                // Live calc preview: show a data output's computed value when it can
                // be evaluated in the editor (pure-node graphs).
                std::string label = p.name;
                if (p.type == PinType::Float)
                    if (auto const v = evalOutput(m_graph, node, p, 0))
                        label += (label.empty() ? "" : "  ") + std::format("= {:g}", *v);

                if (!label.empty())
                {
                    ImGui::TextUnformatted(label.c_str());
                    ImGui::SameLine();
                }
                drawPinIcon(p.type, connected.contains(p.id));
                ed::EndPin();
            }
            ed::EndNode();

            if (!node.placed)
            {
                ed::SetNodePosition(ed::NodeId(node.id), { node.spawnX, node.spawnY });
                node.placed = true;
            }
        }

        // -- links --
        for (auto const& l : m_graph.links)
            ed::Link(ed::LinkId(l.id), ed::PinId(l.from), ed::PinId(l.to));

        // -- create links (opposite kinds + matching type; an input takes one) --
        if (ed::BeginCreate())
        {
            ed::PinId a, b;
            if (ed::QueryNewLink(&a, &b) && a && b)
            {
                auto const* pa = findPin(m_graph, static_cast<int>(a.Get()));
                auto const* pb = findPin(m_graph, static_cast<int>(b.Get()));
                bool const valid = pa && pb && pa->kind != pb->kind && pa->type == pb->type;
                if (valid && ed::AcceptNewItem())
                {
                    bool const aIsOut = (pa->kind == PinKind::Out);
                    int const out = static_cast<int>((aIsOut ? a : b).Get());
                    int const in  = static_cast<int>((aIsOut ? b : a).Get());
                    std::erase_if(m_graph.links, [in](Link const& l) { return l.to == in; });
                    m_graph.links.push_back({ m_graph.id(), out, in });
                }
            }
        }
        ed::EndCreate();

        // -- delete --
        if (ed::BeginDelete())
        {
            ed::LinkId dl;
            while (ed::QueryDeletedLink(&dl))
                if (ed::AcceptDeletedItem())
                {
                    int const id = static_cast<int>(dl.Get());
                    std::erase_if(m_graph.links, [id](Link const& l) { return l.id == id; });
                }
            ed::NodeId dn;
            while (ed::QueryDeletedNode(&dn))
                if (ed::AcceptDeletedItem())
                {
                    int const id = static_cast<int>(dn.Get());
                    std::erase_if(m_graph.nodes, [id](Node const& n) { return n.id == id; });
                }
        }
        ed::EndDelete();

        if (m_fit)
        {
            ed::NavigateToContent();
            m_fit = false;
        }

        // -- right-click canvas: add-node / fit / save / load popup --
        ed::Suspend();
        if (ed::ShowBackgroundContextMenu())
        {
            m_menuX = openPopupPos.x;
            m_menuY = openPopupPos.y;
            ImGui::OpenPopup("##canvasMenu");
        }
        if (ImGui::BeginPopup("##canvasMenu"))
        {
            if (ImGui::BeginMenu("Add Node"))
            {
                for (auto const& spec : registry())
                    if (ImGui::MenuItem(spec.title.c_str()))
                    {
                        ImVec2 const canvas = ed::ScreenToCanvas({ m_menuX, m_menuY });
                        addNode(m_graph, spec, canvas.x, canvas.y);
                    }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Fit to content"))
                m_fit = true;
            if (ImGui::MenuItem("Save + Generate"))
            {
                for (auto& n : m_graph.nodes)
                {
                    ImVec2 const p = ed::GetNodePosition(ed::NodeId(n.id));
                    n.spawnX = p.x;
                    n.spawnY = p.y;
                }
                save(m_graph, "graphs/" + m_name + ".ngraph");

                // Codegen a C# Script the existing scripts pipeline builds + runs.
                // The .cs watcher picks the write up and rebuilds, so the class
                // "Game.<name>" hot-reloads -- a GraphComponent with this graph's
                // name starts running it on the next Play.
                std::error_code ec;
                std::filesystem::create_directories("scripts/generated", ec);
                if (std::ofstream out { "scripts/generated/" + m_name + ".cs" }; out)
                    out << generate(m_graph, m_name);
            }
            if (ImGui::MenuItem("Reload from disk"))
            {
                if (load(m_graph, "graphs/" + m_name + ".ngraph"))
                    m_fit = true;
            }
            ImGui::EndPopup();
        }
        ed::Resume();

        ed::End();
        ed::SetCurrentEditor(nullptr);

        ImGui::End();
    }
} // namespace Editor::Nodes