export module NodeEditor;

import NodeGraph;
import std;

namespace Editor::Nodes
{

    // A dockable node-graph editor panel built on imgui-node-editor. The editor
    // context is opaque (void*) so this interface doesn't drag imgui-node-editor
    // into importers. Edits ONE graph at a time, identified by name:
    // graphs/<name>.ngraph on disk, codegen to scripts/generated/<name>.cs.
    export class NodeEditorPanel
    {
    public:
        NodeEditorPanel() = default;
        ~NodeEditorPanel();
        NodeEditorPanel(NodeEditorPanel const&)            = delete;
        NodeEditorPanel& operator=(NodeEditorPanel const&) = delete;

        // Switch the panel to graph `name`: load graphs/<name>.ngraph if it
        // exists, else start a fresh seeded graph. (The inspector's "Edit
        // Graph" button lands here.)
        void open(std::string const& name);

        // Draw the panel (an ImGui window). `open` toggles visibility.
        void draw(bool* open);

        std::string const& name() const { return m_name; }

    private:
        void*            m_ctx    { nullptr };   // ax::NodeEditor::EditorContext*
        Graph            m_graph;
        std::string      m_name   { "Untitled" };
        bool             m_seeded { false };
        bool             m_fit    { false };     // pending "frame all nodes"
        float            m_menuX  { 0.0f };      // right-click SCREEN pos (spawn point)
        float            m_menuY  { 0.0f };
    };
} // namespace Editor::Nodes