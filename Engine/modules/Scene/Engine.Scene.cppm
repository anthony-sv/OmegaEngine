export module Engine.Scene;

// Scene layer -- the ORCHESTRATION on top of the ECS storage layer.
//
//   Engine.ECS    = data/storage   (Registry, Entity, components)
//   Engine.Scene  = orchestration  (World, SceneManager, later
//                                   serialization / prefabs)
//
// A World is a Registry + the update-phase systems that run on it.
// A SceneManager owns named Worlds and switches the active one.

export import :World;
export import :SceneManager;