export module Engine.Scripting;

// Engine.Scripting -- hosts .NET / C# game scripts in-process.
//
// Gameplay logic lives in C# scripts in the project, not in engine
// components. This module is the bridge:
//   :ScriptHost   -- boots CoreCLR, loads OmegaEngine.dll, marshals calls.
//   :ScriptSystem -- ticks each entity's ScriptComponent into managed code.

export import :ScriptHost;
export import :ScriptSystem;