export module NodeGraph;

import std;

// ─────────────────────────────────────────────────────────────────────────────
//  NodeGraph -- the editor-side data model for a visual script graph.
//
//  A graph is nodes + links. Each node has typed input/output PINS; a link
//  joins an OUT pin to an IN pin. Node/pin/link ids share one integer space
//  (imgui-node-editor wants unique ids across all of them). Pure data (no ImGui
//  / no engine deps) -- the editor renders it, codegen/eval walk it, and it
//  round-trips to a .ngraph file (save/load defined in the .cpp).
// ─────────────────────────────────────────────────────────────────────────────

export namespace Editor::Nodes
{
    // Exec = white control-flow wire; the rest are typed data wires.
    enum class PinType { Exec, Float, Vec2, Bool };
    enum class PinKind { In, Out };

    struct Pin
    {
        int         id   { 0 };
        std::string name {};
        PinType     type { PinType::Float };
        PinKind     kind { PinKind::In };
    };

    struct Node
    {
        int              id    { 0 };
        std::string      type  {};     // spec id, e.g. "OnUpdate"
        std::string      title {};
        std::vector<Pin> inputs;
        std::vector<Pin> outputs;
        float            spawnX { 0.0f };
        float            spawnY { 0.0f };
        bool             placed { false };   // initial position applied?
        double           value  { 0.0 };     // inline number (e.g. a Constant's value)
        std::string      param  {};          // inline choice (e.g. a Key Down's key)
    };

    struct Link
    {
        int id   { 0 };
        int from { 0 };   // an OUT pin id
        int to   { 0 };   // an IN pin id
    };

    // Behavior = events + actions, codegens to a C# Script on save.
    // Calc = a pure value playground, live-evaluated in the editor only
    // (saving writes just the .ngraph -- no class is generated).
    enum class GraphKind { Behavior, Calc };

    // A graph-level variable: becomes a public field on the generated class
    // (default value only for now -- per-entity overrides come later). Read
    // and written in the graph via the Get/Set Variable nodes.
    struct Variable
    {
        enum class Type { Float, Bool };

        std::string name {};
        Type        type { Type::Float };
        double      def  { 0.0 };   // bools: 0 = false, else true
    };

    struct Graph
    {
        std::vector<Node>     nodes;
        std::vector<Link>     links;
        std::vector<Variable> variables;
        GraphKind             kind { GraphKind::Behavior };
        int                   next { 1 };

        int id() { return next++; }
    };

    // A node TYPE in the palette: a title, its pin layout, and its inline
    // editors -- a draggable number (the Constant) and/or a choice combo
    // (Key Down's key). Non-empty `options` makes the editor draw the combo.
    struct Spec
    {
        std::string type;
        std::string title;
        std::vector<std::pair<std::string, PinType>> inputs;
        std::vector<std::pair<std::string, PinType>> outputs;
        bool        hasValue { false };
        std::vector<std::string> options {};
    };

    // The node library. Names in `options` must match the C# `Key` enum
    // members (codegen emits `Input.IsKeyDown(Key.<param>)` verbatim).
    inline std::vector<Spec> const& registry()
    {
        static std::vector<Spec> const r = {
            // events -- one method each in the generated class
            { "OnCreate", "On Create", {},                                                 {{ "", PinType::Exec }} },
            { "OnUpdate", "On Update", {},                                                 {{ "", PinType::Exec }, { "dt", PinType::Float }} },
            { "OnCollisionEnter", "On Collision Enter", {},                                {{ "", PinType::Exec }, { "Normal", PinType::Vec2 }} },
            { "OnCollisionExit",  "On Collision Exit",  {},                                {{ "", PinType::Exec }, { "Normal", PinType::Vec2 }} },
            { "OnTriggerEnter",   "On Trigger Enter",   {},                                {{ "", PinType::Exec }} },
            { "OnTriggerExit",    "On Trigger Exit",    {},                                {{ "", PinType::Exec }} },

            // data sources
            { "Constant", "Constant",  {},                                                 {{ "Value", PinType::Float }},  true },
            { "Time",     "Time",      {},                                                 {{ "Elapsed", PinType::Float }} },
            { "KeyDown",  "Key Down",  {},                                                 {{ "Down", PinType::Bool }},    false,
              { "W", "A", "S", "D", "Space", "Left", "Right", "Up", "Down", "Enter", "Escape", "LeftShift" } },
            { "MouseDown", "Mouse Down", {},                                               {{ "Down", PinType::Bool }},    false,
              { "Left", "Right", "Middle" } },
            { "MousePos", "Mouse Position", {},                                            {{ "Position", PinType::Vec2 }} },
            { "GetPosition", "Get Position", {},                                           {{ "Position", PinType::Vec2 }} },

            // variables (param = the variable's name; the panel offers the
            // graph's declared variables of the matching type)
            { "GetVar",  "Get Variable",        {},                                        {{ "Value", PinType::Float }} },
            { "GetVarB", "Get Variable (Bool)", {},                                        {{ "Value", PinType::Bool }} },
            { "SetVar",  "Set Variable",        {{ "", PinType::Exec }, { "Value", PinType::Float }}, {{ "", PinType::Exec }} },
            { "SetVarB", "Set Variable (Bool)", {{ "", PinType::Exec }, { "Value", PinType::Bool }},  {{ "", PinType::Exec }} },

            // math / comparison
            { "Add",      "Add",       {{ "A", PinType::Float }, { "B", PinType::Float }},  {{ "Result", PinType::Float }} },
            { "Multiply", "Multiply",  {{ "A", PinType::Float }, { "B", PinType::Float }},  {{ "Result", PinType::Float }} },
            { "Greater",  "Greater",   {{ "A", PinType::Float }, { "B", PinType::Float }},  {{ "A > B", PinType::Bool }} },

            // vec2 plumbing + math
            { "MakeVec2",  "Make Vec2",   {{ "X", PinType::Float }, { "Y", PinType::Float }}, {{ "V", PinType::Vec2 }} },
            { "SplitVec2", "Split Vec2",  {{ "V", PinType::Vec2 }},                           {{ "X", PinType::Float }, { "Y", PinType::Float }} },
            { "AddVec2",   "Add (Vec2)",  {{ "A", PinType::Vec2 }, { "B", PinType::Vec2 }},   {{ "Result", PinType::Vec2 }} },
            { "ScaleVec2", "Scale (Vec2)", {{ "V", PinType::Vec2 }, { "S", PinType::Float }}, {{ "Result", PinType::Vec2 }} },

            // control flow
            { "Branch",   "Branch",    {{ "", PinType::Exec }, { "Condition", PinType::Bool }},
                                                                                            {{ "True", PinType::Exec }, { "False", PinType::Exec }} },

            // actions -- statements on the exec chain
            { "SetVelocity", "Set Velocity", {{ "", PinType::Exec }, { "X", PinType::Float }, { "Y", PinType::Float }},
                                                                                            {{ "", PinType::Exec }} },
            { "SetVelocityV", "Set Velocity (Vec2)", {{ "", PinType::Exec }, { "V", PinType::Vec2 }},
                                                                                            {{ "", PinType::Exec }} },
            { "Move",     "Move",      {{ "", PinType::Exec }, { "X", PinType::Float }, { "Y", PinType::Float }},
                                                                                            {{ "", PinType::Exec }} },
            { "MoveV",    "Move (Vec2)", {{ "", PinType::Exec }, { "V", PinType::Vec2 }},
                                                                                            {{ "", PinType::Exec }} },
            { "Print",    "Print",     {{ "", PinType::Exec }, { "Value", PinType::Float }}, {{ "", PinType::Exec }} },

            // debug -- calc-graph value probe (editor-only; never codegens)
            { "Watch",    "Watch",     {{ "Value", PinType::Float }},                       {} },
        };
        return r;
    }

    inline Spec const* findSpec(std::string_view type)
    {
        for (auto const& s : registry())
            if (s.type == type)
                return &s;
        return nullptr;
    }

    // Instantiate `spec` into `g` at (x, y); returns the new node.
    inline Node& addNode(Graph& g, Spec const& spec, float x, float y)
    {
        Node n;
        n.id     = g.id();
        n.type   = spec.type;
        n.title  = spec.title;
        n.spawnX = x;
        n.spawnY = y;
        if (!spec.options.empty())
            n.param = spec.options.front();
        for (auto const& [name, type] : spec.inputs)
            n.inputs.push_back({ g.id(), name, type, PinKind::In });
        for (auto const& [name, type] : spec.outputs)
            n.outputs.push_back({ g.id(), name, type, PinKind::Out });
        g.nodes.push_back(std::move(n));
        return g.nodes.back();
    }

    // Look up a pin by id across every node.
    inline Pin const* findPin(Graph const& g, int pinId)
    {
        for (auto const& n : g.nodes)
        {
            for (auto const& p : n.inputs)  if (p.id == pinId) return &p;
            for (auto const& p : n.outputs) if (p.id == pinId) return &p;
        }
        return nullptr;
    }

    // Serialization (.ngraph JSON) -- defined in NodeGraph.cpp.
    bool save(Graph const& g, std::filesystem::path const& file);
    bool load(Graph& g, std::filesystem::path const& file);
}