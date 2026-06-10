module NodeCodegen;

import NodeGraph;
import std;

namespace Editor::Nodes
{
    namespace
    {
        Node const* ownerOfInput(Graph const& g, int pinId)
        {
            for (auto const& n : g.nodes)
                for (auto const& p : n.inputs)
                    if (p.id == pinId)
                        return &n;
            return nullptr;
        }

        std::pair<Node const*, Pin const*> ownerOfOutput(Graph const& g, int pinId)
        {
            for (auto const& n : g.nodes)
                for (auto const& p : n.outputs)
                    if (p.id == pinId)
                        return { &n, &p };
            return { nullptr, nullptr };
        }

        // The OUT pin feeding `inPinId`, or -1 if unconnected.
        int sourceOf(Graph const& g, int inPinId)
        {
            for (auto const& l : g.links)
                if (l.to == inPinId)
                    return l.from;
            return -1;
        }

        // The node whose exec-IN is wired from `execOutPinId`.
        Node const* execNext(Graph const& g, int execOutPinId)
        {
            for (auto const& l : g.links)
                if (l.from == execOutPinId)
                    return ownerOfInput(g, l.to);
            return nullptr;
        }

        int firstExecOut(Node const& n)
        {
            for (auto const& p : n.outputs)
                if (p.type == PinType::Exec)
                    return p.id;
            return -1;
        }

        // A C# float literal, shortest round-trip (`3` -> `3f`, `3.5` -> `3.5f`).
        std::string fmtNum(double v)
        {
            return std::format("{}f", v);
        }

        std::string exprOfOutput(Graph const& g, Node const& n, Pin const& out);

        // The C# expression flowing into an input pin (or a typed default if open).
        std::string exprOfInput(Graph const& g, Pin const& in)
        {
            std::string const fallback = (in.type == PinType::Bool) ? "false" : "0f";
            int const src = sourceOf(g, in.id);
            if (src < 0)
                return fallback;              // unconnected data input
            auto const [n, p] = ownerOfOutput(g, src);
            return (n && p) ? exprOfOutput(g, *n, *p) : fallback;
        }

        std::string exprOfOutput(Graph const& g, Node const& n, Pin const& out)
        {
            if (n.type == "Constant")
                return fmtNum(n.value);
            if (n.type == "OnUpdate" && out.name == "dt")
                return "dt";
            if (n.type == "Time")
                return "Time.Elapsed";
            if (n.type == "KeyDown")
                return "Input.IsKeyDown(Key." + n.param + ")";
            if (n.type == "Add" && n.inputs.size() >= 2)
                return "(" + exprOfInput(g, n.inputs[0]) + " + " + exprOfInput(g, n.inputs[1]) + ")";
            if (n.type == "Multiply" && n.inputs.size() >= 2)
                return "(" + exprOfInput(g, n.inputs[0]) + " * " + exprOfInput(g, n.inputs[1]) + ")";
            if (n.type == "Greater" && n.inputs.size() >= 2)
                return "(" + exprOfInput(g, n.inputs[0]) + " > " + exprOfInput(g, n.inputs[1]) + ")";
            return (out.type == PinType::Bool) ? "false" : "0f";
        }

        // The expressions feeding a node's Float inputs, in pin order
        // (SetVelocity / Move read their X and Y this way).
        std::vector<std::string> floatArgs(Graph const& g, Node const& n)
        {
            std::vector<std::string> args;
            for (auto const& in : n.inputs)
                if (in.type == PinType::Float)
                    args.push_back(exprOfInput(g, in));
            return args;
        }

        std::string statementOf(Graph const& g, Node const& n, std::string const& pad)
        {
            if (n.type == "Print")
            {
                for (auto const& in : n.inputs)
                    if (in.type != PinType::Exec)
                        return pad + "Console.WriteLine(" + exprOfInput(g, in) + ");\n";
                return pad + "Console.WriteLine();\n";
            }
            if (n.type == "SetVelocity")
            {
                auto const a = floatArgs(g, n);
                if (a.size() >= 2)
                    return pad + "Entity.Velocity = new Vector2(" + a[0] + ", " + a[1] + ");\n";
            }
            if (n.type == "Move")
            {
                auto const a = floatArgs(g, n);
                if (a.size() >= 2)
                    return pad + "Entity.Position += new Vector2(" + a[0] + ", " + a[1] + ");\n";
            }
            return {};
        }

        // Emit the statement chain reachable from an exec-OUT pin (a Sequence).
        // Branch forks into an if/else (each arm a sub-chain) and ENDS the
        // linear walk -- there is no merge node, so nothing follows it.
        std::string emitChain(Graph const& g, int execOutPinId, std::string const& pad)
        {
            std::string s;
            Node const* cur = execNext(g, execOutPinId);
            for (int guard = 0; cur != nullptr && guard < 1024; ++guard)
            {
                if (cur->type == "Branch")
                {
                    std::string cond = "false";
                    for (auto const& in : cur->inputs)
                        if (in.type == PinType::Bool)
                            cond = exprOfInput(g, in);

                    int trueOut = -1, falseOut = -1;
                    for (auto const& p : cur->outputs)
                        if (p.type == PinType::Exec)
                            (p.name == "True" ? trueOut : falseOut) = p.id;

                    s += pad + "if (" + cond + ")\n" + pad + "{\n"
                       + emitChain(g, trueOut, pad + "    ")
                       + pad + "}\n";
                    if (std::string const arm = emitChain(g, falseOut, pad + "    "); !arm.empty())
                        s += pad + "else\n" + pad + "{\n" + arm + pad + "}\n";
                    break;
                }

                s += statementOf(g, *cur, pad);
                int const next = firstExecOut(*cur);
                cur = (next >= 0) ? execNext(g, next) : nullptr;
            }
            return s;
        }
    }

    std::string generate(Graph const& g, std::string const& className)
    {
        std::string o;
        o += "using System.Numerics;\n";
        o += "using OmegaEngine;\n\n";
        o += "namespace Game;\n\n";
        o += "// AUTO-GENERATED from a node graph. Do not edit by hand.\n";
        o += "public sealed class " + className + " : Script\n{\n";

        auto emitEvent = [&](Node const& n, std::string const& signature)
        {
            o += "    public override void " + signature + "\n    {\n";
            if (int const ex = firstExecOut(n); ex >= 0)
                o += emitChain(g, ex, "        ");
            o += "    }\n\n";
        };

        for (auto const& n : g.nodes)
        {
            if (n.type == "OnCreate")      emitEvent(n, "OnCreate()");
            else if (n.type == "OnUpdate") emitEvent(n, "OnUpdate(float dt)");
        }

        o += "}";
        return o;
    }
}