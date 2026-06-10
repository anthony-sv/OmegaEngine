module;

#include "nlohmann/json.hpp"

module NodeGraph;

import std;

namespace Editor::Nodes
{
    using json = nlohmann::json;

    namespace
    {
        json pinJson(Pin const& p)
        {
            return { { "id", p.id }, { "name", p.name }, { "type", static_cast<int>(p.type) } };
        }

        Pin pinFrom(json const& j, PinKind kind)
        {
            return {
                j.value("id", 0),
                j.value("name", std::string {}),
                static_cast<PinType>(j.value("type", 1)),
                kind,
            };
        }
    }

    bool save(Graph const& g, std::filesystem::path const& file)
    {
        json root;
        root["next"] = g.next;

        json nodes = json::array();
        for (auto const& n : g.nodes)
        {
            json jn;
            jn["id"]    = n.id;
            jn["type"]  = n.type;
            jn["title"] = n.title;
            jn["x"]     = n.spawnX;
            jn["y"]     = n.spawnY;
            jn["value"] = n.value;
            if (!n.param.empty())
                jn["param"] = n.param;

            json ins = json::array();
            for (auto const& p : n.inputs)  ins.push_back(pinJson(p));
            json outs = json::array();
            for (auto const& p : n.outputs) outs.push_back(pinJson(p));
            jn["inputs"]  = std::move(ins);
            jn["outputs"] = std::move(outs);

            nodes.push_back(std::move(jn));
        }
        root["nodes"] = std::move(nodes);

        json links = json::array();
        for (auto const& l : g.links)
            links.push_back({ { "id", l.id }, { "from", l.from }, { "to", l.to } });
        root["links"] = std::move(links);

        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
        std::ofstream out(file);
        if (!out)
            return false;
        out << root.dump(2);
        return true;
    }

    bool load(Graph& g, std::filesystem::path const& file)
    {
        std::ifstream in(file);
        if (!in)
            return false;

        json root;
        try { in >> root; }
        catch (...) { return false; }

        g = Graph {};
        g.next = root.value("next", 1);

        for (auto const& jn : root.value("nodes", json::array()))
        {
            Node n;
            n.id     = jn.value("id", 0);
            n.type   = jn.value("type", std::string {});
            n.title  = jn.value("title", std::string {});
            n.spawnX = jn.value("x", 0.0f);
            n.spawnY = jn.value("y", 0.0f);
            n.value  = jn.value("value", 0.0);
            n.param  = jn.value("param", std::string {});
            n.placed = false;
            for (auto const& jp : jn.value("inputs", json::array()))  n.inputs.push_back(pinFrom(jp, PinKind::In));
            for (auto const& jp : jn.value("outputs", json::array())) n.outputs.push_back(pinFrom(jp, PinKind::Out));
            g.nodes.push_back(std::move(n));
        }

        for (auto const& jl : root.value("links", json::array()))
            g.links.push_back({ jl.value("id", 0), jl.value("from", 0), jl.value("to", 0) });

        return true;
    }
}