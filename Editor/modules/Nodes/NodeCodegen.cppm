export module NodeCodegen;

import NodeGraph;
import std;

// Behavior-graph backend: walk a NodeGraph and emit a C# `Script` subclass. Each
// event node (On Create / On Update) becomes a method; following its EXEC wire
// emits the statement chain, and each statement's DATA inputs recurse into the
// upstream value nodes as C# expressions. The result rides the existing scripts
// pipeline (build + hot reload), so a graph runs exactly like a hand-written
// script.
namespace Editor::Nodes
{
    // Generate the full C# source for `g` as class `className` (namespace Game).
    export std::string generate(Graph const& g, std::string const& className);
}